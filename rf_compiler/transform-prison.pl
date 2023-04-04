#!/usr/bin/env perl

# changed above from globalsymbol_regex = r'^[^.]\w+:' -- Mike

$localsymbol_regex = '^\.[\w.]+:';

$globalsymbol_regex = '^[^.].*:';
$commsymbol_regex = '^\s+\.comm.+';
$weakrefsymbol_regex = '^\s+\.weakref.+';
$weaksymbol_regex = '^\s+\.weak.+';
$setsymbol_regex = '^\s+\.set.+';
$filesymbol_regex = '^\s+\.file.+';

$call_regex = 'call.+__rf_handle_.+';

$mov_regex = 'mov.*\s+\$__rf_signal';
$mov_search_regex='\$__rf_signal';
$mov_repl_src='$0';
$mov_repl_dst='$1';


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


open(IN,"$infile") or die "cannot open $infile\n";
open(OUT,">$outfile") or die "cannot open $outfile\n";

@lines = <IN>;

print STDERR "Read files and computed skip regexps\n";


map {
    @matches = ($_ =~ m/$localsymbol_regex/g);
    if ($#matches>=0) { 
	chop(@matches);
#	print "Adding local labels: ". join("\n", @matches),"\n";
	push @local_labels,  @matches;
    }
} @lines;

map { $local_label_hash{$_}=1; } @local_labels;
$local_label_regexp = join("|", map { "(".$_.")"} @local_labels);

$n_labels = $#local_labels+1;
$n_keys = keys %local_label_hash;
$n_regexp = length($local_label_regexp);

$n_dest_lines_modded=0;

print STDERR "Built local label database ($n_labels in list, $n_keys in hash, regexp has size $n_regexp)\n";
$i=0; map { emit_src_line($_); $i++;} @lines;

print OUT "\n\n";

print STDERR "Emitted source lines\n";

$i=0; map { emit_dst_line($_); $i++; print STDERR "emitted destination line $i ($n_dest_lines_modded modified)\r" if !($i % 1000); } @lines;

print STDERR "Emitted $i destination lines ($n_dest_lines_modded modified)\n";
print OUT "\n\n";
print OUT ".data\n";
print OUT "rollforward_table:\n";

$size=0; $i=0; map {
    if (should_emit_rf_label($_)) { 
        print OUT "  .quad __RF_SRC_$i, __RF_DST_$i\n";
	$size++;
    }
    $i++;
} @lines;

print STDERR "Emitted rollforward table\n";


print OUT "rollback_table:\n";

$i=0; map { 
    if (should_emit_rf_label($_)) { 
        print OUT "  .quad __RF_DST_$i, __RF_SRC_$i\n";
    }
    $i++;
} @lines;

print OUT "rollforward_table_size: .quad $size\n";
print OUT "rollback_table_size: .quad $size\n";

print STDERR "Emitted rollback table\n";

print STDERR "Done\n";


sub should_emit_rf_label
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
    if ($sl =~/$mov_regex/) {
	chomp($sl);
	$sl =~ s/$mov_search_regex/$mov_repl_src/;
	$sl .= "  # updated prison check to $mov_repl_src\n";
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

    # PAD acceleration attempt 1 - about 20% faster
    if (0) {
	# avoid doing repeated scans of the line unless it matches
	# at least one of the labels
	if ($sl =~ /$local_label_regexp/) { 
	    foreach my $lbl (@local_labels) {
		$sl =~ s/$lbl/$lbl\_RF/g;
	    }
	}
    }

    # PAD acceleration attempt 2 - about a billion times faster
    if (1) {
	# this may miss some stuff...
	# it slices the line into pieces that might be terminals (probably misses some)
	# and then searches for the terminals in a hash of all local labels
	my @w = split(/\s+|\:|\,|\-\+\(/,$sl);
#	print STDERR "$sl - |",join("|",@w),"|\n";
	my @found; map { push @found, $_ if $local_label_hash{$_}; } @w;
	if ($#found>=0) {
	    map { $sl =~ s/$_/$_\_RF/g } @found;
	    $n_dest_lines_modded++;
	}
    }

    # Maybe use a trie to represent the patterns and the line?
    # Maybe actually parse the assembly line, and then catch things attempt 2 might miss
    # Or possibly an effort that does substrings?



	
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

    if ($sl =~/$mov_regex/) {
	chomp($sl);
	$sl =~ s/$mov_search_regex/$mov_repl_dst/;
	$sl .= "  # updated prison check to $mov_repl_dst\n";
    }
    

    print OUT $sl;
}


