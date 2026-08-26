#!/usr/bin/env python3
"""Generate the bulk UTF-8 decoder's DFA tables from the streaming decoder.

The tables are not described by hand: every state transition and emission is
derived by simulating the byte path (Utf8Decoder::pushByte in utf8.cpp plus
the ground dispatcher's handling of printable ASCII and stray C1) over
decoder states that carry the accumulator as an interval.  Any input whose
behaviour would depend on accumulator bits the DFA does not track fails an
assertion here, at build time, instead of miscompiling into the tables.

Usage: generate_utf8_dfa.py OUTPUT_HEADER
"""

import sys
from dataclasses import dataclass

# Emission opcodes, mirrored by the decoding loop in vterm.cpp.
COUNT_MASK = 3
FIRST_BYTE = 4  # first emission is the input byte itself
SECOND_BYTE = 8  # second emission is the input byte itself
SLOW = 16  # emission is the completed codepoint: needs its properties
STOP = 32  # a traced build leaves the byte to the ground dispatcher
RESET = 64  # stray C1 resets grapheme input before its replacement

# A stray ground C1 is one action with two readings: a traced build stops
# the run before the emission so the trace layer reports a control event,
# production takes the replacement emission and stays in the run.
ACTIONS = {
    (): 0,
    ("byte",): 1 | FIRST_BYTE,
    ("fffd",): 1,
    ("stop",): 1 | STOP | RESET,
    ("cp",): 1 | SLOW,
    ("fffd", "fffd"): 2,
    ("fffd", "byte"): 2 | SECOND_BYTE,
}


@dataclass(frozen=True)
class State:
    """A streaming-decoder state with the accumulator as an interval.

    pending mirrors the parser's dumb groundUtf8Remaining counter after the
    decoder aborts a sequence early: the trace layer still counts the
    aborted sequence's continuation bytes as text, so a C1 there is not a
    stray control."""

    remaining: int
    low: int
    high: int
    minimum: int
    pending: int = 0

    def accumulator_is(self, value):
        if self.low == value and self.high == value:
            return True
        if self.low <= value <= self.high:
            raise AssertionError(
                f"accumulator interval [{self.low:#x}, {self.high:#x}] straddles "
                f"{value:#x}: the DFA cannot express this state")
        return False


GROUND = State(0, 0, 0, 0)


def ground_pending(count):
    return GROUND if count == 0 else State(0, 0, 0, 0, count)


def is_exit(byte):
    return byte < 0x20 or byte == 0x7F


def lead_state(byte):
    if 0xC2 <= byte <= 0xDF:
        return State(1, byte & 0x1F, byte & 0x1F, 0x80)
    if 0xE0 <= byte <= 0xEF:
        return State(2, byte & 0x0F, byte & 0x0F, 0x800)
    if 0xF0 <= byte <= 0xF4:
        return State(3, byte & 0x07, byte & 0x07, 0x10000)
    return None


def step(state, byte):
    """The byte path's behaviour: returns (emissions, next state).

    Transcribes Utf8Decoder::pushByte and the ground dispatcher byte for
    byte; every branch below must correspond to a branch there.
    """
    if is_exit(byte):
        raise AssertionError("exit bytes never reach the simulated decoder")

    if byte < 0x80:
        # inputGraphicChar: checkPrematureEOS flushes a pending sequence,
        # then the printable byte is placed.
        if state.remaining > 0:
            return ("fffd", "byte"), GROUND
        return ("byte",), GROUND

    if (byte & 0xC0) == 0x80:
        if state.remaining == 0:
            if state.pending > 0:
                # The trace layer still owes this byte to the aborted
                # sequence; the decoder sees a stray and replaces it.
                return ("fffd",), ground_pending(state.pending - 1)
            # Stray continuation. A C1 byte stays observable as a control
            # event: the run stops so the ground dispatcher handles it.
            if byte <= 0x9F:
                return ("stop",), GROUND
            return ("fffd",), GROUND
        invalid_first = (
            (state.remaining == 2 and state.accumulator_is(0) and byte < 0xA0)
            or (state.remaining == 2 and state.accumulator_is(0x0D) and byte >= 0xA0)
            or (state.remaining == 3 and state.accumulator_is(0) and byte < 0x90)
            or (state.remaining == 3 and state.accumulator_is(4) and byte >= 0x90))
        if invalid_first:
            return ("fffd", "fffd"), ground_pending(state.remaining - 1)
        low = (state.low << 6) | (byte & 0x3F)
        high = (state.high << 6) | (byte & 0x3F)
        if state.remaining > 1:
            return (), State(state.remaining - 1, low, high, state.minimum)
        # Completion: the whole interval must land in range, or the DFA
        # would need the accumulator's value to pick the emission.
        valid = (low >= state.minimum and high <= 0x10FFFF
                 and (high < 0xD800 or low > 0xDFFF))
        invalid = high < state.minimum or low > 0x10FFFF or (
            low >= 0xD800 and high <= 0xDFFF)
        if valid:
            return ("cp",), GROUND
        if invalid:
            return ("fffd",), GROUND
        raise AssertionError(
            f"completion interval [{low:#x}, {high:#x}] straddles the valid "
            f"range above {state.minimum:#x}")

    # Not a continuation: flush any pending sequence, then start over.
    flushed = ("fffd",) if state.remaining > 0 else ()
    lead = lead_state(byte)
    if lead is not None:
        return flushed, lead
    return flushed + ("fffd",), GROUND


def discover():
    """Enumerate reachable states, widening continuation payloads so the
    state space is finite: a successor's interval covers every continuation
    byte the predecessor accepts into the same behaviour."""
    bytes_all = [byte for byte in range(256) if not is_exit(byte)]
    states = [GROUND]
    index = {GROUND: 0}
    transitions = []  # per state: {byte: (emissions, target id)}
    frontier = 0
    while frontier < len(states):
        state = states[frontier]
        row = {}
        # Group continuation successors: all continuations from this state
        # that behave identically must target one widened state.
        cont_groups = {}
        for byte in bytes_all:
            emissions, target = step(state, byte)
            if (byte & 0xC0) == 0x80 and state.remaining > 1 and emissions == ():
                cont_groups.setdefault(emissions, []).append((byte, target))
                continue
            row[byte] = (emissions, target)
        for emissions, members in cont_groups.items():
            low = min(target.low for _, target in members)
            high = max(target.high for _, target in members)
            remaining = members[0][1].remaining
            minimum = members[0][1].minimum
            assert all(t.remaining == remaining and t.minimum == minimum for _, t in members)
            widened = State(remaining, low, high, minimum)
            for byte, _ in members:
                row[byte] = (emissions, widened)
        resolved = {}
        for byte, (emissions, target) in row.items():
            if target not in index:
                index[target] = len(states)
                states.append(target)
            resolved[byte] = (emissions, index[target])
        transitions.append(resolved)
        frontier += 1
    return states, transitions, bytes_all


def minimize(states, transitions, bytes_all):
    """Partition refinement: merge states with identical behaviour."""
    signature = [tuple(transitions[s][b][0] for b in bytes_all) for s in range(len(states))]
    blocks = {}
    partition = []
    for s in range(len(states)):
        partition.append(blocks.setdefault(signature[s], len(blocks)))
    while True:
        refined_blocks = {}
        refined = []
        for s in range(len(states)):
            key = (partition[s], tuple(
                (transitions[s][b][0], partition[transitions[s][b][1]]) for b in bytes_all))
            refined.append(refined_blocks.setdefault(key, len(refined_blocks)))
        if refined == partition:
            return partition, len(refined_blocks)
        partition = refined


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)

    states, transitions, bytes_all = discover()
    partition, state_count = minimize(states, transitions, bytes_all)

    # Ground must be state 0, and every state the decoding loop must not
    # rewind at a run boundary — the decoder is in ground, only the trace
    # counter is pending — numbers below the true mid-sequence states.
    block_no_rewind = {}
    for s in range(len(states)):
        block_no_rewind.setdefault(partition[s], set()).add(states[s].remaining == 0)
    assert all(len(kinds) == 1 for kinds in block_no_rewind.values())
    order = {}
    for s in range(len(states)):
        block = partition[s]
        if block not in order and True in block_no_rewind[block]:
            order[block] = len(order)
    rewind_first = len(order)
    for s in range(len(states)):
        order.setdefault(partition[s], len(order))
    partition = [order[block] for block in partition]
    assert partition[0] == 0

    # Byte classes: bytes with identical behaviour in every state.
    representatives = {}
    for block in range(state_count):
        for s in range(len(states)):
            if partition[s] == block:
                representatives[block] = s
                break
    def column(byte):
        if is_exit(byte):
            return "exit"
        return tuple(
            (transitions[representatives[block]][byte][0],
             partition[transitions[representatives[block]][byte][1]])
            for block in range(state_count))
    class_of_column = {}
    byte_class = [0] * 256
    columns = []
    for byte in range(256):
        col = column(byte)
        if col not in class_of_column:
            class_of_column[col] = len(columns)
            columns.append(col)
        byte_class[byte] = class_of_column[col]

    # Order classes: exit first, leads (a class is a lead when ground moves
    # to a non-ground state on it) last, so the loop tests both with one
    # comparison each.
    def is_lead_class(cls):
        col = columns[cls]
        if col == "exit":
            return False
        _, target = col[0]
        return target != 0
    lead_classes = [cls for cls in range(len(columns)) if is_lead_class(cls)]
    exit_class = class_of_column["exit"]
    rest = [cls for cls in range(len(columns)) if cls != exit_class and cls not in lead_classes]
    new_order = {exit_class: 0}
    for cls in rest + lead_classes:
        new_order[cls] = len(new_order)
    byte_class = [new_order[cls] for cls in byte_class]
    class_count = len(columns)
    lead_first = 1 + len(rest)

    # Lead masks, taken from the lead's significant bits.
    mask = [0] * class_count
    for byte in range(256):
        lead = lead_state(byte)
        if lead is None:
            continue
        bits = {1: 0x1F, 2: 0x0F, 3: 0x07}[lead.remaining]
        cls = byte_class[byte]
        assert cls >= lead_first
        assert mask[cls] in (0, bits)
        mask[cls] = bits

    # The tables.
    next_table = [[0] * class_count for _ in range(state_count)]
    act_table = [[0] * class_count for _ in range(state_count)]
    for block in range(state_count):
        s = representatives[block]
        for byte in range(256):
            cls = byte_class[byte]
            if cls == 0:
                next_table[block][cls] = block
                act_table[block][cls] = 0
                continue
            emissions, target = transitions[s][byte]
            next_table[block][cls] = partition[target]
            act_table[block][cls] = ACTIONS[emissions]

    # Sanity: the state and class counts the loop is tuned for.
    assert state_count == 10, state_count
    assert class_count == 13, class_count

    with open(sys.argv[1], "w") as header:
        header.write("// Generated by generate_utf8_dfa.py from the streaming\n")
        header.write("// decoder's semantics; do not edit.\n")
        header.write("#pragma once\n\n")
        header.write("#include <std/sys/types.h>\n\n")
        header.write("namespace Utf8Dfa {\n")
        header.write(f"    constexpr u8 CountMask = {COUNT_MASK};\n")
        header.write(f"    constexpr u8 FirstByte = {FIRST_BYTE};\n")
        header.write(f"    constexpr u8 SecondByte = {SECOND_BYTE};\n")
        header.write(f"    constexpr u8 Slow = {SLOW};\n")
        header.write(f"    constexpr u8 Stop = {STOP};\n")
        header.write(f"    constexpr u8 Reset = {RESET};\n\n")
        header.write("    constexpr u8 Exit = 0;\n")
        header.write(f"    constexpr u8 LeadFirst = {lead_first};\n")
        header.write("    constexpr u8 Ground = 0;\n")
        header.write(f"    constexpr u8 RewindFirst = {rewind_first};\n")
        header.write(f"    constexpr u8 stateCount = {state_count};\n")
        header.write(f"    constexpr u8 classCount = {class_count};\n\n")
        def emit_array(name, values, per_line):
            header.write(f"    inline constexpr u8 {name}[{len(values)}] = {{\n")
            for start in range(0, len(values), per_line):
                chunk = ", ".join(str(v) for v in values[start:start + per_line])
                header.write(f"        {chunk},\n")
            header.write("    };\n\n")
        emit_array("cls", byte_class, 16)
        def emit_table(name, table):
            header.write(f"    inline constexpr u8 {name}[{state_count}][{class_count}] = {{\n")
            for row in table:
                header.write("        {" + ", ".join(str(v) for v in row) + "},\n")
            header.write("    };\n\n")
        emit_table("next", next_table)
        emit_table("act", act_table)
        emit_array("mask", mask, 16)
        # The trace-counter debt a run hands back to the parser when it
        # ends in a state below RewindFirst.
        pending = [0] * state_count
        for s in range(len(states)):
            pending[partition[s]] = states[s].pending
        emit_array("pending", pending, 16)
        header.write("}\n")


if __name__ == "__main__":
    main()
