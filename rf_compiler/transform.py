#!/usr/bin/env python3

import re
import sys

infile = open(sys.argv[1], 'r')
out = open(sys.argv[2], 'w')

localsymbol_regex = r'^\.[\w.]+:'
globalsymbol_regex = r'^[^.].+:'
# changed above from globalsymbol_regex = r'^[^.]\w+:' -- Mike
commsymbol_regex = r'^\s+\.comm.+'
weakrefsymbol_regex = r'^\s+\.weakref.+'
weaksymbol_regex = r'^\s+\.weak.+'
setsymbol_regex = r'^\s+\.set.+'
filesymbol_regexp = r'^\s+\.file.+'

lines = list(infile)
# Find all the labels that match `.XX` (.LBBN_M for example)
# these labels need to be renamed in the rollforward version
# of the code.
local_labels = []
for line in lines:
    matches = re.findall(localsymbol_regex, line)
    for match in matches:
        local_labels.append(match[:-1])

#call_re = re.compile('call(\w+)__rf_handle_(\w+)')
call_re = re.compile('call.+__rf_handle_.+')


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
    m = re.search(call_re, srcline)
    if m is not None:
        srcline = f'# removed handler call: {srcline}'
    out.write(srcline)

def emit_dst_line(line):
    if should_emit_rf_label(line):
        out.write(f'__RF_DST_{i}:')
    srcline = line
    for lbl in local_labels:
        srcline = srcline.replace(lbl, f'{lbl}_RF')
    srcline = re.sub(globalsymbol_regex, '# removed', srcline)
    srcline = re.sub(commsymbol_regex, '# removed', srcline)
    srcline = re.sub(weakrefsymbol_regex, '# removed', srcline)
    srcline = re.sub(weaksymbol_regex, '# removed', srcline)
    srcline = re.sub(setsymbol_regex, '# removed', srcline)
    srcline = re.sub(filesymbol_regexp, '# removed', srcline)

    m = re.search(call_re, srcline)
    # replace calls to functions that look like __rf_handle_*
    #if m is not None:
        # I commented out the _in_rf suffix b/c it was causing the assembler to fail --Mike
        #srcline = '# removed handler call'
        #srcline = srcline.replace(m.groups()[0], m.groups()[0])
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
    if should_emit_rf_label(line):
        out.write(f'  .quad __RF_SRC_{i}, __RF_DST_{i}\n')
        size += 1
    
out.write('\n\n')
out.write('rollback_table:\n')
for i, line in enumerate(lines):
    if should_emit_rf_label(line):
        out.write(f'  .quad __RF_DST_{i}, __RF_SRC_{i}\n')
out.write(f'rollforward_table_size: .quad {size}\n')
out.write(f'rollback_table_size: .quad {size}\n')
