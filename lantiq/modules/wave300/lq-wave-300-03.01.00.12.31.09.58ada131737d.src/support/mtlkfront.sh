#!/bin/sh

rebaser_subdir=$1
shift

abs_top_srcdir=`readlink -f $abs_top_srcdir`
abs_builddir=`readlink -f $abs_builddir`

if test x"$mtlkfront_do_filtering" = x"y"; then
FILTER_CMD=do_filtering
else
FILTER_CMD=cat
fi

do_filtering () {

perl -e '
    use strict;

    my $builddir=path_to_regexp($ARGV[0]);
    my $srcdir=$ARGV[1];
    
    while (<STDIN>)
    {
      my $line=$_;
      my @file_names = ();
      
      #Get all substrings that look like file names from the string
      if(@file_names = ($line =~ /[\w\d\.\/\-]+\/+/g))
      {
        foreach my $file_name (@file_names)
        {
           my $full_name=trim(`([ -e $file_name ] && readlink -f $file_name) || echo $file_name`);
           my $file_name_regexp = path_to_regexp($file_name);
           $line =~ s/$file_name_regexp/$full_name\//;
        }
      }
      
      $line =~ s/$builddir/$srcdir/g;
      #Remove multiple consequent slashes
      $line =~ s/([a-zA-Z0-9_\.\-])\/+([a-zA-Z0-9_\.\-])/\1\/\2/g;
      print "$line";
    }

    sub trim($)
    {
        my $string = shift;
        $string =~ s/^\s+//;
        $string =~ s/[\s\r]+$//;
        return $string;
    }

    sub path_to_regexp($)
    {
      my $path = shift;
      $path =~ s/([\:\\\/\.\,\-])/\\$1/g;
      return $path;
    }' "$abs_builddir/$rebaser_subdir" "$abs_top_srcdir/"
}

{ $* 2>&1; echo $?>.$$.pipe.result; } | $FILTER_CMD

RESULT_VAL=`cat .$$.pipe.result`
rm -rf .$$.pipe.result
exit $RESULT_VAL
