#!/usr/bin/env python3
"""Reject parser grammars that can strand the scanner in the error state.

feed() drives the generated machine with `while (p != pe)` and the ragel
error state consumes nothing, so a single byte without a transition wedges
the terminal forever.  The grammar contract is therefore the same one vte
gets from Rust's exhaustive `match`: every state must either cover all 256
bytes or route the leftover bytes through an error-recovery action that
fgoto's a live machine (the `$err(...) { fhold; fgoto ...; }` idiom).

The check runs on ragel's backend-independent XML dump, so it holds for
both the -G1 production build and the -T1 test build.

Usage: check_parser_totality.py <parser.rl> [stamp file written on success]
"""

import subprocess
import sys
from html.parser import HTMLParser


# A tiny pure-python element tree: xml.etree needs pyexpat, which homebrew
# pythons ship linked against a libexpat newer than the system dylib and
# fail to import.  Ragel's -x dump is machine-generated, lowercase and
# well-formed, well within html.parser's power.
class Node:
    def __init__(self, tag, attrs):
        self.tag = tag
        self.attrib = dict(attrs)
        self.children = []
        self.text = ""

    def find(self, path):
        node = self
        for part in path.split("/"):
            node = next(
                (child for child in node.children if child.tag == part), None
            )
            if node is None:
                return None
        return node

    def get(self, key):
        return self.attrib.get(key)

    def __iter__(self):
        return iter(self.children)


class TreeBuilder(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = Node("", ())
        self.stack = [self.root]

    def handle_starttag(self, tag, attrs):
        node = Node(tag, attrs)
        self.stack[-1].children.append(node)
        self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.stack[-1].children.append(Node(tag, attrs))

    def handle_endtag(self, tag):
        while len(self.stack) > 1:
            if self.stack.pop().tag == tag:
                break

    def handle_data(self, data):
        self.stack[-1].text += data


def parse_xml(text):
    builder = TreeBuilder()
    builder.feed(text)
    builder.close()
    return builder.root.children[0]


def main():
    source = sys.argv[1]
    xml_text = subprocess.run(
        ["ragel", "-x", source, "-o", "/dev/stdout"],
        check=True, capture_output=True, text=True,
    ).stdout
    machine = parse_xml(xml_text).find("ragel_def/machine")

    # Action tables reference actions; a recovery action is one that
    # rewrites cs (<goto>/<goto_expr>/<next>/<next_expr> or <call>/<ret>).
    recovering_actions = set()
    for action in machine.find("action_list"):
        for op in ("goto", "goto_expr", "next", "next_expr", "call", "ret"):
            if action.find(op) is not None:
                recovering_actions.add(action.get("id"))
    recovering_tables = set()
    for table in machine.find("action_table_list"):
        ids = (table.text or "").split()
        if any(i in recovering_actions for i in ids):
            recovering_tables.add(table.get("id"))

    failures = []
    for state in machine.find("state_list"):
        state_id = state.get("id")
        if state_id == "0":
            continue  # the error state itself (parser_error in the output)
        covered = 0
        trans_list = state.find("trans_list")
        transitions = list(trans_list) if trans_list is not None else []
        for t in transitions:
            lower, upper, target, action = t.text.split()
            covered += int(upper) - int(lower) + 1
            if target == "x" and action not in recovering_tables:
                failures.append(
                    f"state {state_id}: bytes {lower}..{upper} hit the error "
                    f"state without a recovery fgoto"
                )
        if covered < 256:
            failures.append(
                f"state {state_id}: only {covered}/256 bytes have transitions"
            )

    if failures:
        print(f"{source}: parser state machine is not total:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    if len(sys.argv) > 2:
        with open(sys.argv[2], "w") as stamp:
            stamp.write("total\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
