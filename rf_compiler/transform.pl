#!/usr/bin/perl -w

# changed above from globalsymbol_regex = r'^[^.]\w+:' -- Mike

$localsymbol_regex = '^\.[\w.]+:';

$globalsymbol_regex = '^[^.].+:';
$commsymbol_regex = '^\s+\.comm.+';
$weakrefsymbol_regex = '^\s+\.weakref.+';
$weaksymbol_regex = '^\s+\.weak.+';
$setsymbol_regex = '^\s+\.set.+';
$filesymbol_regex = '^\s+\.file.+';

$call_regex = 'call.+__rf_handle_.+';

@skip_regexps = (
    $globalsymbol_regex,
    $commsymbol_regex,
    $weakrefsymbol_regex,
    $weaksymbol_regex,
    $setsymbol_regex,
    $filesymbol_regex
    # add more here
    );
    
$skip_regexp = join("|", map { "(".$_.")"} @skip_regexps);


$#ARGV==1 or die "usage: transform.pl infile outfile\n";

$infile = shift;
$outfile = shift;

print STDERR "skip_regexp=$skip_regexp\n";

open(IN,"$infile") or die "cannot open $infile\n";
open(OUT,">$outfile") or die "cannot open $outfile\n";

@lines = <IN>;
foreach $l (@lines) {
    @matches = ($l =~ m/$localsymbol_regex/g);
    push @local_labels, @matches;
}

$i=0;
foreach $l (@lines) {
    emit_src_line($l);
    $i++;
}

print OUT "\n\n";

$i=0;
foreach $l (@lines) {
    emit_dst_line($l);
    $i++;
}

print OUT "\n\n";
print OUT ".data\n";
print OUT "rollforward_table:\n";

$size = 0;
$i=0;
foreach $l (@lines) {
    if (should_emit_rf_label($l)) { 
        print OUT "  .quad __RF_SRC_$i, __RF_DST_$i\n";
	$size++;
    }
}

print OUT "rollback_table:\n";

$i=0;
foreach $l (@lines) {
    if (should_emit_rf_label($l)) { 
        print OUT "  .quad __RF_DST_$i, __RF_SRC_$i\n";
    }
}

print OUT 'rollforward_table_size: .quad $size\n';
print OUT 'rollback_table_size: .quad $size\n';



sub should_emit_rflabel
{
    my $l = shift;
    if ($l =~ /^\s*\./) {
	return 0;
    } else {
	return !($l =~ /$globalsymbol_regex/);
    }
}


sub emit_src_line
{
    my $l = shift;
    if (should_emit_rf_label($l)) {
	print OUT "__RF_SRC_$i: ";
    }
    my $sl = $l;
    if ($sl =~ /$call_regex/) {
	$sl = "# removed handler call: $sl";
    }
    print OUT $sl;
}


sub emit_dst_line
{
    my $l = shift;
    if (should_emit_rf_label($l)) {
	print OUT "__RF_DST_$i: ";
    }
    my $sl = $l;

    foreach my $lbl (@local_labels) {
	$sl =~ s/$lbl/$lbl\_RF/g;
    }
	
    $sl =~ s/$skip_regexp/\#removed/g;

    if ($sl =~ /$call_regex/) {
	#m = re.search(call_re, srcline)
	# replace calls to functions that look like __rf_handle_*
	#if m is not None:
        # I commented out the _in_rf suffix b/c it was causing the assembler to fail --Mike
        #srcline = '# removed handler call'
        #srcline = srcline.replace(m.groups()[0], m.groups()[0])
	# PERL
	# $sl = "# removed handler call";
	# $sl =~  ... ?
    }

    print OUT $sl;
}


# lines = list(infile)
# w# Find all the labels that match `.XX` (.LBBN_M for example)
# # these labels need to be renamed in the rollforward version
# # of the code.
# local_labels = []
# for line in lines:
#     matches = re.findall(localsymbol_regex, line)
#     for match in matches:
#         local_labels.append(match[:-1])

# #call_re = re.compile('call(\w+)__rf_handle_(\w+)')
# call_re = re.compile('call.+__rf_handle_.+')


# def should_emit_rf_label(line):
#     if line.lstrip().startswith('.'):
#         return False
#     if re.match(globalsymbol_regex, line) is None:
#         return True

#     return False

# def emit_src_line(line):
#     if should_emit_rf_label(line):
#         out.write(f'__RF_SRC_{i}:')
#     srcline = line
#     m = re.search(call_re, srcline)
#     if m is not None:
#         srcline = f'# removed handler call: {srcline}'
#     out.write(srcline)

# def emit_dst_line(line):
#     if should_emit_rf_label(line):
#         out.write(f'__RF_DST_{i}:')
#     srcline = line
#     for lbl in local_labels:
#         srcline = srcline.replace(lbl, f'{lbl}_RF')
#     srcline = re.sub(globalsymbol_regex, '# removed', srcline)
#     srcline = re.sub(commsymbol_regex, '# removed', srcline)
#     srcline = re.sub(weakrefsymbol_regex, '# removed', srcline)
#     srcline = re.sub(weaksymbol_regex, '# removed', srcline)
#     srcline = re.sub(setsymbol_regex, '# removed', srcline)

#     m = re.search(call_re, srcline)
#     # replace calls to functions that look like __rf_handle_*
#     #if m is not None:
#         # I commented out the _in_rf suffix b/c it was causing the assembler to fail --Mike
#         #srcline = '# removed handler call'
#         #srcline = srcline.replace(m.groups()[0], m.groups()[0])
#     out.write(srcline)


# for i, line in enumerate(lines):
#     emit_src_line(line)
# out.write('\n\n')

# for i, line in enumerate(lines):
#     emit_dst_line(line)
# out.write('\n\n')

# out.write('\n\n')
# out.write('.data\n')
# out.write('rollforward_table:\n')
# size = 0
# for i, line in enumerate(lines):
#     if should_emit_rf_label(line):
#         out.write(f'  .quad __RF_SRC_{i}, __RF_DST_{i}\n')
#         size += 1
    
# out.write('\n\n')
# out.write('rollback_table:\n')
# for i, line in enumerate(lines):
#     if should_emit_rf_label(line):
#         out.write(f'  .quad __RF_DST_{i}, __RF_SRC_{i}\n')
# out.write(f'rollforward_table_size: .quad {size}\n')
# out.write(f'rollback_table_size: .quad {size}\n')
