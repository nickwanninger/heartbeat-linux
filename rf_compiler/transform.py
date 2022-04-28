#!/usr/bin/env python3

import re
import sys

infile = open(sys.argv[1], 'r')
out = open(sys.argv[2], 'w')

localsymbol_regex = r'^\.[\w.]+:'
globalsymbol_regex = r'^[^.]\w+:'

lines = list(infile)
# Find all the labels that match `.XX` (.LBBN_M for example)
# these labels need to be renamed in the rollforward version
# of the code.
local_labels = []
for line in lines:
    matches = re.findall(localsymbol_regex, line)
    for match in matches:
        local_labels.append(match[:-1])

# The handlers to look for
handlers = [
    'square'
]

handler_call_matches = list(map(lambda a: re.compile(f'callq\s+{a}'), handlers))

def should_emit_rf_label(line):
    if line.lstrip().startswith('.'):
        return False
    if re.match(globalsymbol_regex, line) is None:
        return True


    return False

def emit_src_line(line):
    if should_emit_rf_label(line):
        out.write(f'__RF_SRC_{i}:')
    srcline = line
    for h in handler_call_matches:
        srcline = re.sub(h, '# removed handler call', srcline)
    out.write(srcline)

def emit_dst_line(line):
    if should_emit_rf_label(line):
        out.write(f'__RF_DST_{i}:')
    srcline = line
    for lbl in local_labels:
        srcline = srcline.replace(lbl, f'{lbl}_RF')
    srcline = re.sub(globalsymbol_regex, '# removed', srcline)
    for h in handler_call_matches:
        srcline = re.sub(h, '# removed call', srcline)
    out.write(srcline)


for i, line in enumerate(lines):
    emit_src_line(line)
out.write('\n\n')

for i, line in enumerate(lines):
    emit_dst_line(line)
out.write('\n\n')

out.write('\n\n')
out.write('.data\n')
out.write('rollforward_table:\n')
size = 0
for i, line in enumerate(lines):
    size += 1
    if should_emit_rf_label(line):
        out.write(f'  .quad __RF_SRC_{i}, __RF_DST_{i}\n')
    
out.write('\n\n')
out.write('rollback_table:\n')
for i, line in enumerate(lines):
    if should_emit_rf_label(line):
        out.write(f'  .quad __RF_DST_{i}, __RF_SRC_{i}\n')
out.write(f'rollforward_table_size: .quad {size}\n')
