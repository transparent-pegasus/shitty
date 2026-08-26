/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 *
 * Test tool: parses a TOML document from stdin and prints the toml-test
 * JSON encoding of it, or exits non-zero on any parse or semantic error.
 * The parser proper is a syntax-level SAX machine; the table/array
 * redefinition rules of TOML live in the consumer, and this tool carries
 * the full set so the toml-test corpus exercises both layers.
 *
 * The document model is flat and trivially destructible: every node is a
 * record in one vector, every string a slice of one arena, so a whole
 * document resets in a few clears and nothing can leak no matter where an
 * invalid document breaks off.
 */

#include <lib/vterm/num.h>
#include "toml.h"

#include <std/lib/buffer.h>
#include <std/lib/vector.h>
#include <std/str/view.h>
#include <std/ios/fs_utils.h>
#include <std/ios/in_fd.h>
#include <std/ios/sys.h>
#include <std/str/builder.h>
#include <std/str/fmt.h>
#include <std/sys/fd.h>
#include <std/sys/throw.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace stl;

namespace {
    constexpr i32 none = -1;

    enum class NodeKind : u8 {
        Table,
        Array,
        Scalar
    };

    struct NodeRec {
        NodeKind kind;
        TomlType type;
        bool inlineClosed;
        bool explicitHeader;
        bool dotted;
        bool tableArray;
        u32 textOffset;
        u32 textLength;
        u32 nameOffset;
        u32 nameLength;
        i32 firstChild;
        i32 lastChild;
        i32 nextSibling;
    };

    struct SpanRec {
        u32 offset;
        u32 length;
    };

    struct FrameRec {
        i32 node;
        u32 pathBase;
        u32 pathCount;
    };

    struct DomSink final: public TomlSink {
        Buffer arena;
        Vector<NodeRec> nodes;
        Vector<SpanRec> paths;
        Vector<FrameRec> stack;
        u32 rootPathBase;
        u32 rootPathCount;
        i32 current;
        bool broken;

        DomSink();

        void reset();
        i32 makeNode(NodeKind kind);
        u32 store(StringView text, u32& length);
        StringView slice(u32 offset, u32 length) const;
        i32 findChild(i32 table, StringView name) const;
        i32 addChild(i32 table, StringView name, NodeKind kind);
        void adopt(i32 table, i32 child);

        bool tomlTable(const StringView* path, size_t count, bool array) override;
        bool tomlKey(const StringView* path, size_t count) override;
        bool tomlScalar(TomlType type, StringView text) override;
        bool tomlArrayBegin() override;
        bool tomlArrayEnd() override;
        bool tomlInlineTableBegin() override;
        bool tomlInlineTableEnd() override;
        void tomlError(size_t line, StringView message) override;

        bool place(i32 node);
        bool attach(i32 table, u32 pathBase, u32 pathCount, i32 node);
    };
}

DomSink::DomSink()
    : rootPathBase(0)
    , rootPathCount(0)
    , current(0)
    , broken(false)
{
    reset();
}

void DomSink::reset() {
    arena.reset();
    nodes.clear();
    paths.clear();
    stack.clear();
    rootPathBase = 0;
    rootPathCount = 0;
    broken = false;
    current = makeNode(NodeKind::Table);
}

i32 DomSink::makeNode(NodeKind kind) {
    NodeRec node = {};
    node.kind = kind;
    node.type = TomlType::String;
    node.firstChild = none;
    node.lastChild = none;
    node.nextSibling = none;
    nodes.pushBack(node);
    return (i32)(nodes.length() - 1);
}

u32 DomSink::store(StringView text, u32& length) {
    const u32 offset = (u32)(arena.used());
    arena.append(text.data(), text.length());
    length = (u32)(text.length());
    return offset;
}

StringView DomSink::slice(u32 offset, u32 length) const {
    return StringView((const u8*)(arena.data()) + offset, (size_t)(length));
}

i32 DomSink::findChild(i32 table, StringView name) const {
    for (i32 child = nodes[table].firstChild; child != none; child = nodes[child].nextSibling) {
        if (slice(nodes[child].nameOffset, nodes[child].nameLength) == name) {
            return child;
        }
    }
    return none;
}

void DomSink::adopt(i32 table, i32 child) {
    if (nodes[table].lastChild == none) {
        nodes.mut(table).firstChild = child;
    } else {
        nodes.mut(nodes[table].lastChild).nextSibling = child;
    }
    nodes.mut(table).lastChild = child;
}

i32 DomSink::addChild(i32 table, StringView name, NodeKind kind) {
    const i32 child = makeNode(kind);
    u32 length = 0;
    const u32 offset = store(name, length);
    nodes.mut(child).nameOffset = offset;
    nodes.mut(child).nameLength = length;
    adopt(table, child);
    return child;
}

bool DomSink::tomlTable(const StringView* path, size_t count, bool array) {
    i32 at = 0;
    for (size_t index = 0; index + 1 < count; ++index) {
        i32 next = findChild(at, path[index]);
        if (next == none) {
            next = addChild(at, path[index], NodeKind::Table);
        }
        // Traversing THROUGH a dotted-defined table on the way to a deeper
        // header is legal; only naming one as the header target is not.
        if (nodes[next].kind == NodeKind::Array && nodes[next].tableArray) {
            next = nodes[next].lastChild;
        } else if (nodes[next].kind != NodeKind::Table || nodes[next].inlineClosed) {
            return false;
        }
        at = next;
    }
    i32 target = findChild(at, path[count - 1]);
    if (array) {
        if (target == none) {
            target = addChild(at, path[count - 1], NodeKind::Array);
            nodes.mut(target).tableArray = true;
        }
        if (nodes[target].kind != NodeKind::Array || !nodes[target].tableArray) {
            return false;
        }
        const i32 element = makeNode(NodeKind::Table);
        nodes.mut(element).explicitHeader = true;
        adopt(target, element);
        current = element;
        return true;
    }
    if (target == none) {
        target = addChild(at, path[count - 1], NodeKind::Table);
        nodes.mut(target).explicitHeader = true;
        current = target;
        return true;
    }
    if (nodes[target].kind != NodeKind::Table || nodes[target].inlineClosed || nodes[target].dotted || nodes[target].explicitHeader) {
        return false;
    }
    nodes.mut(target).explicitHeader = true;
    current = target;
    return true;
}

bool DomSink::tomlKey(const StringView* path, size_t count) {
    const u32 base = (u32)(paths.length());
    for (size_t index = 0; index < count; ++index) {
        SpanRec span = {};
        span.offset = store(path[index], span.length);
        paths.pushBack(span);
    }
    if (stack.empty()) {
        rootPathBase = base;
        rootPathCount = (u32)(count);
    } else {
        stack.mutBack().pathBase = base;
        stack.mutBack().pathCount = (u32)(count);
    }
    return true;
}

bool DomSink::tomlScalar(TomlType type, StringView text) {
    const i32 node = makeNode(NodeKind::Scalar);
    nodes.mut(node).type = type;
    u32 length = 0;
    const u32 offset = store(text, length);
    nodes.mut(node).textOffset = offset;
    nodes.mut(node).textLength = length;
    return place(node);
}

bool DomSink::tomlArrayBegin() {
    stack.pushBack({makeNode(NodeKind::Array), 0, 0});
    return true;
}

bool DomSink::tomlArrayEnd() {
    const i32 node = stack.popBack().node;
    return place(node);
}

bool DomSink::tomlInlineTableBegin() {
    stack.pushBack({makeNode(NodeKind::Table), 0, 0});
    return true;
}

bool DomSink::tomlInlineTableEnd() {
    const i32 node = stack.popBack().node;
    nodes.mut(node).inlineClosed = true;
    return place(node);
}

bool DomSink::place(i32 node) {
    if (stack.empty()) {
        return attach(current, rootPathBase, rootPathCount, node);
    }
    const FrameRec& frame = stack.back();
    if (nodes[frame.node].kind == NodeKind::Array) {
        adopt(frame.node, node);
        return true;
    }
    return attach(frame.node, frame.pathBase, frame.pathCount, node);
}

bool DomSink::attach(i32 table, u32 pathBase, u32 pathCount, i32 node) {
    // The path segment bytes already sit in the arena; nodes reference the
    // stored spans directly, which also keeps the arena from appending a
    // slice of itself mid-reallocation.
    i32 at = table;
    for (u32 index = 0; index + 1 < pathCount; ++index) {
        const SpanRec span = paths[pathBase + index];
        i32 next = findChild(at, slice(span.offset, span.length));
        if (next == none) {
            next = makeNode(NodeKind::Table);
            nodes.mut(next).nameOffset = span.offset;
            nodes.mut(next).nameLength = span.length;
            nodes.mut(next).dotted = true;
            adopt(at, next);
        } else if (nodes[next].kind != NodeKind::Table || nodes[next].inlineClosed || nodes[next].explicitHeader || !nodes[next].dotted) {
            return false;
        }
        at = next;
    }
    const SpanRec span = paths[pathBase + pathCount - 1];
    if (findChild(at, slice(span.offset, span.length)) != none) {
        return false;
    }
    nodes.mut(node).nameOffset = span.offset;
    nodes.mut(node).nameLength = span.length;
    adopt(at, node);
    return true;
}

void DomSink::tomlError(size_t line, StringView message) {
    sysE << StringView(u8"toml: line ") << line << StringView(u8": ") << message << endL;
    broken = true;
}

namespace {
    static void printJsonString(StringBuilder& out, StringView text) {
        out << StringView(u8"\"");
        for (const u8 byte : text) {
            if (byte == '"' || byte == '\\') {
                const u8 pair[2] = {'\\', byte};
                out << StringView(pair, 2);
            } else if (byte == '\n') {
                out << StringView(u8"\\n");
            } else if (byte == '\r') {
                out << StringView(u8"\\r");
            } else if (byte == '\t') {
                out << StringView(u8"\\t");
            } else if (byte < 0x20 || byte == 0x7f) {
                static const char digits[] = "0123456789abcdef";
                const u8 escaped[6] = {'\\', 'u', '0', '0', (u8)(digits[byte >> 4]), (u8)(digits[byte & 15])};
                out << StringView(escaped, 6);
            } else {
                out << StringView(&byte, 1);
            }
        }
        out << StringView(u8"\"");
    }

    static bool renderInteger(StringView text, StringBuilder& out) {
        i64 parsed = 0;
        if (text.length() > 2 && (text[1] == 'x' || text[1] == 'o' || text[1] == 'b')) {
            const unsigned base = text[1] == 'x' ? 16 : text[1] == 'o' ? 8 : 2;
            u64 wide = 0;
            if (!parseU64(text.suffix(text.length() - 2), wide, base) || wide > (u64)(INT64_MAX)) {
                return false;
            }
            parsed = (i64)(wide);
        } else if (!parseI64(text, parsed)) {
            return false;
        }
        char digits[24];
        const size_t length = (const char*)(formatI64Base10(parsed, digits)) - digits;
        out << StringView((const u8*)(digits), length);
        return true;
    }

    static void renderFloat(StringView text, StringBuilder& out) {
        double parsed = 0.0;
        parseF64(text, parsed);
        if (parsed != parsed) {
            out << StringView(u8"nan");
            return;
        }
        if (parsed > 1.7976931348623157e308) {
            out << StringView(u8"inf");
            return;
        }
        if (parsed < -1.7976931348623157e308) {
            out << StringView(u8"-inf");
            return;
        }
        char digits[48];
        const size_t length = formatF64Roundtrip(parsed, digits) - digits;
        out << StringView((const u8*)(digits), length);
    }

    static const char* typeName(TomlType type) {
        switch (type) {
            case TomlType::String:
                return "string";
            case TomlType::Integer:
                return "integer";
            case TomlType::Float:
                return "float";
            case TomlType::Boolean:
                return "bool";
            case TomlType::OffsetDatetime:
                return "datetime";
            case TomlType::LocalDatetime:
                return "datetime-local";
            case TomlType::LocalDate:
                return "date-local";
            case TomlType::LocalTime:
                return "time-local";
        }
        return "string";
    }

    // Out-of-range integers are the one error only visible at encoding
    // time; find them before printing so batch output framing never sees
    // a half-printed document.
    static bool validNode(const DomSink& sink, i32 node) {
        const NodeRec& record = sink.nodes[node];
        if (record.kind == NodeKind::Scalar) {
            StringBuilder scratch;
            return record.type != TomlType::Integer || renderInteger(sink.slice(record.textOffset, record.textLength), scratch);
        }
        for (i32 child = record.firstChild; child != none; child = sink.nodes[child].nextSibling) {
            if (!validNode(sink, child)) {
                return false;
            }
        }
        return true;
    }

    static void printNode(StringBuilder& out, const DomSink& sink, i32 node) {
        const NodeRec& record = sink.nodes[node];
        if (record.kind == NodeKind::Scalar) {
            const StringView text = sink.slice(record.textOffset, record.textLength);
            out << StringView(u8"{\"type\":");
            printJsonString(out, StringView(typeName(record.type)));
            out << StringView(u8",\"value\":");
            if (record.type == TomlType::Integer) {
                StringBuilder rendered;
                renderInteger(text, rendered);
                printJsonString(out, StringView(rendered));
            } else if (record.type == TomlType::Float) {
                StringBuilder rendered;
                renderFloat(text, rendered);
                printJsonString(out, StringView(rendered));
            } else {
                printJsonString(out, text);
            }
            out << StringView(u8"}");
            return;
        }
        if (record.kind == NodeKind::Array) {
            out << StringView(u8"[");
            bool first = true;
            for (i32 child = record.firstChild; child != none; child = sink.nodes[child].nextSibling) {
                if (!first) {
                    out << StringView(u8",");
                }
                first = false;
                printNode(out, sink, child);
            }
            out << StringView(u8"]");
            return;
        }
        out << StringView(u8"{");
        bool first = true;
        for (i32 child = record.firstChild; child != none; child = sink.nodes[child].nextSibling) {
            if (!first) {
                out << StringView(u8",");
            }
            first = false;
            printJsonString(out, sink.slice(sink.nodes[child].nameOffset, sink.nodes[child].nameLength));
            out << StringView(u8":");
            printNode(out, sink, child);
        }
        out << StringView(u8"}");
    }

    static bool parseDocument(const Buffer& input, DomSink& sink) {
        sink.reset();
        if (!parseToml(StringView(input), sink)) {
            if (!sink.broken) {
                sysE << StringView(u8"toml: document breaks table or key rules") << endL;
            }
            return false;
        }
        if (!validNode(sink, 0)) {
            sysE << StringView(u8"toml: integer out of range") << endL;
            return false;
        }
        return true;
    }

    static bool readFile(const char* path, Buffer& input) {
        input.reset();
        Buffer filename{StringView(path)};
        try {
            readFileContent(filename, input);
        } catch (Exception&) {
            return false;
        }
        return true;
    }
}

// Two modes: with no arguments, one document on stdin, its JSON (or exit 1)
// out. With file arguments, every file is parsed by this one process and
// reported as its own line - "<path>\tok\t<json>" or "<path>\terror" - so a
// test run over the whole corpus pays for one process, not seven hundred
// (an hour of sanitizer startups otherwise).
int main(int argc, char** argv) {
    DomSink sink;
    if (argc > 1) {
        Buffer input;
        for (int index = 1; index < argc; ++index) {
            // One flushed line per file: an abort mid-batch must not eat
            // the finished reports before it.
            if (!readFile(argv[index], input) || !parseDocument(input, sink)) {
                sysO << StringView(argv[index]) << StringView(u8"\terror") << endL << flsH;
                continue;
            }
            StringBuilder rendered;
            printNode(rendered, sink, 0);
            sysO << StringView(argv[index]) << StringView(u8"\tok\t") << StringView(rendered) << endL << flsH;
        }
        return 0;
    }
    Buffer input;
    {
        // A non-owning descriptor: stdin stays with the process.
        FD in(0);
        FDInput(in).readAll(input);
    }
    if (!parseDocument(input, sink)) {
        return 1;
    }
    StringBuilder rendered;
    printNode(rendered, sink, 0);
    sysO << StringView(rendered) << endL;
    return 0;
}
