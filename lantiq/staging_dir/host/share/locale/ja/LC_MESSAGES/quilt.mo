Þ          ´  Å   L	      `  ô   a  Z   V  ~  ±  à  0  «    }  ½  ª   ;  m   æ  r   T  Õ   Ç  Ê    n  h  ù   ×  [   Ñ  `   -  N     \   Ý  k  :  |  ¦$  p  #+  ²  -  @   G/  '  /    °0     Ì1     ã1  %   ö1  *   2     G2  6   [2     2  B   °2  E   ó2  L   93     3     3  $   ¨3  E   Í3  !   4     54     M4  0   c4     4      °4  -   Ñ4     ÿ4     5  ,   75  !   d5     5      ¢5     Ã5     ß5      ð5     6     /6     O6     l6     6  -   ¦6      Ô6  %   õ6  #   7     ?7  0   T7  2   7  $   ¸7     Ý7  H   ø7     A8     Q8     d8     y8     8     ¦8  =   ¸8  D   ö8  $   ;9  (   `9  '   9  !   ±9  +   Ó9  .   ÿ9     .:  B   H:  9   :  3   Å:     ù:  #   ;     7;     M;     m;     ;     £;     ¼;  '   Ô;  &   ü;     #<     <<  ,   T<  A   <     Ã<     Ø<     ë<  0   ÿ<  1   0=     b=  #   y=     =  %   »=  #   á=  <   >  &   B>  O   i>  ê   ¹>  3   ¤?  X   Ø?  ,   1@     ^@  @   u@  (   ¶@  )   ß@     	A  0   (A  ¶   YA     B  =   -B     kB  ]   B  *   èB  d   C     xC     C  "   ³C  '   ÖC     þC  ³   D  )   ÒD     üD     E     4E      GE     hE  7   E  .   ·E  /   æE  ï   F  N  G  g   UH  å  ½H  î  £K    P  &  ¤R    ËT  Ò   ÜU  ¥   ¯V  è   UW  C  >X  ü  Z    \  M   ^     Y^  S   ö^  t   J_  ñ  ¿_    ±f    Ho    èr  k   wu    ãu  C  gw     «x     Âx  0   Úx  f   y  '   ry  d   y  :   ÿy  V   :z  k   z  a   ýz     _{     s{  2   {     Ä{  .   I|  5   x|  6   ®|  I   å|  <   /}  <   l}  S   ©}  -   ý}  3   +~  K   _~  G   «~  -   ó~  -   !  7   O  0     F   ¸  0   ÿ  0   0  @   a  <   ¢  R   ß  f   2  7     C   Ñ  K     $   a  ]     F   ä  #   +  6   O  µ     #   <  2   `  2     A   Æ  6     $   ?  Y   d  V   ¾  :     9   P  <     B   Ç  `   
  K   k  $   ·     Ü  T   b  T   ·  -     Y   :  6     0   Ë  -   ü  1   *  *   \  *     B   ²  F   õ  7   <     t  3     e   Ç  0   -  $   ^  !     B   ¥  B   è  0   +  C   \  0      ?   Ñ  $     t   6  A   «  ~   í  e  l  D   Ò       ,   ³  -   à  D     0   S  1     &   ¶  8   Ý  Ê     (   á  I   
  "   T  e   w  3   Ý  l     %   ~  #   ¤  .   È  2   ÷  '   *  Â   R  -        C      b       (        Ã  Y   Þ  A   8  A   z     \   +   %       6          3   R             U   J   N   :   }          Y   a   I   Z      C           n   _      O   T       "            8            ~   o   h   d          9   ?       M         F       t          ,   v          u   B       5       b   V                                          X      .          l   ^   $          -   G   !   <   W       H       r   S       1      y      z   @             g      {                  [          )   *       /   #   P                  7           j   e   k      4   q   A   
          ;              Q          x       w   f      |   ]   &             0   '                    i      K   	       >                     E      s   (       D           =   L   c   2   p   m   `          
Add one or more files to the topmost or named patch.  Files must be
added to the patch before being modified.  Files that are modified by
patches already applied on top of the specified patch cannot be added.

-P patch
	Patch to add files to.
 
Edit the specified file(s) in \$EDITOR (%s) after adding it (them) to
the topmost patch.
 
Fork the topmost patch.  Forking a patch means creating a verbatim copy
of it under a new name, and use that new name instead of the original
one in the current series.  This is useful when a patch has to be
modified, but the original version of it should be preserved, e.g.
because it is used in another series, or for the history.  A typical
sequence of commands would be: fork, edit, refresh.

If new_name is missing, the name of the forked patch will be the current
patch name, followed by \`-2'.  If the patch name already ends in a
dash-and-number, the number is further incremented (e.g., patch.diff,
patch-2.diff, patch-3.diff).
 
Generate a dot(1) directed graph showing the dependencies between
applied patches. A patch depends on another patch if both touch the same
file or, with the --lines option, if their modifications overlap. Unless
otherwise specified, the graph includes all patches that the topmost
patch depends on.
When a patch name is specified, instead of the topmost patch, create a
graph for the specified patch. The graph will include all other patches
that this patch depends on, as well as all patches that depend on this
patch.

--all	Generate a graph including all applied patches and their
	dependencies. (Unapplied patches are not included.)

--reduce
	Eliminate transitive edges from the graph.

--lines[=num]
	Compute dependencies by looking at the lines the patches modify.
	Unless a different num is specified, two lines of context are
	included.

--edge-labels=files
	Label graph edges with the file names that the adjacent patches
	modify.

-T ps	Directly produce a PostScript output file.
 
Global options:

--trace
	Runs the command in bash trace mode (-x). For internal debugging.

--quiltrc file
	Use the specified configuration file instead of ~/.quiltrc (or
	/etc/quilt.quiltrc if ~/.quiltrc does not exist).  See the pdf
	documentation for details about its possible contents.  The
	special value \"-\" causes quilt not to read any configuration
	file.

--version
	Print the version number and exit immediately. 
Grep through the source files, recursively, skipping patches and quilt
meta-information. If no filename argument is given, the whole source
tree is searched. Please see the grep(1) manual page for options.

-h	Print this help. The grep -h option can be passed after a
	double-dash (--). Search expressions that start with a dash
	can be passed after a second double-dash (-- --).
 
Please remove all patches using \`quilt pop -a' from the quilt version used to create this working tree, or remove the %s directory and apply the patches from scratch.\n 
Print a list of applied patches, or all patches up to and including the
specified patch in the file series.
 
Print a list of patches that are not applied, or all patches that follow
the specified patch in the series file.
 
Print an annotated listing of the specified file showing which
patches modify which lines. Only applied patches are included.

-P patch
	Stop checking for changes at the specified rather than the
	topmost patch.
 
Print or change the header of the topmost or specified patch.

-a, -r, -e
	Append to (-a) or replace (-r) the exiting patch header, or
	edit (-e) the header in \$EDITOR (%s). If none of these options is
	given, print the patch header.

--strip-diffstat
	Strip diffstat output from the header.

--strip-trailing-whitespace
	Strip trailing whitespace at the end of lines of the header.

--backup
	Create a backup copy of the old version of a patch as patch~.
 
Print the list of files that the topmost or specified patch changes.

-a	List all files in all applied patches.

-l	Add patch name to output.

-v	Verbose, more user friendly output.

--combine patch
	Create a listing for all patches between this patch and
	the topmost or specified patch. A patch name of \`-' is
	equivalent to specifying the first applied patch.

 
Print the list of patches that modify the specified file. (Uses a
heuristic to determine which files are modified by unapplied patches.
Note that this heuristic is much slower than scanning applied patches.)

-v	Verbose, more user friendly output.
 
Print the name of the next patch after the specified or topmost patch in
the series file.
 
Print the name of the previous patch before the specified or topmost
patch in the series file.
 
Print the name of the topmost patch on the current stack of applied
patches.
 
Print the names of all patches in the series file.

-v	Verbose, more user friendly output.
 
Produces a diff of the specified file(s) in the topmost or specified
patch.  If no files are specified, all files that are modified are
included.

-p n	Create a -p n style patch (-p0 or -p1 are supported).

-p ab	Create a -p1 style patch, but use a/file and b/file as the
	original and new filenames instead of the default
	dir.orig/file and dir/file names.

-u, -U num, -c, -C num
	Create a unified diff (-u, -U) with num lines of context. Create
	a context diff (-c, -C) with num lines of context. The number of
	context lines defaults to 3.

--no-timestamps
	Do not include file timestamps in patch headers.

--no-index
	Do not output Index: lines.

-z	Write to standard output the changes that have been made
	relative to the topmost or specified patch.

-R	Create a reverse diff.

-P patch
	Create a diff for the specified patch.  (Defaults to the topmost
	patch.)

--combine patch
	Create a combined diff for all patches between this patch and
	the patch specified with -P. A patch name of \`-' is equivalent
	to specifying the first applied patch.

--snapshot
	Diff against snapshot (see \`quilt snapshot -h').

--diff=utility
	Use the specified utility for generating the diff. The utility
	is invoked with the original and new file name as arguments.

--color[=always|auto|never]
	Use syntax coloring.

--sort	Sort files by their name instead of preserving the original order.
 
Refreshes the specified patch, or the topmost patch by default.
Documentation that comes before the actual patch in the patch file is
retained.

It is possible to refresh patches that are not on top.  If any patches
on top of the patch to refresh modify the same files, the script aborts
by default.  Patches can still be refreshed with -f.  In that case this
script will print a warning for each shadowed file, changes by more
recent patches will be ignored, and only changes in files that have not
been modified by any more recent patches will end up in the specified
patch.

-p n	Create a -p n style patch (-p0 or -p1 supported).

-p ab	Create a -p1 style patch, but use a/file and b/file as the
	original and new filenames instead of the default
	dir.orig/file and dir/file names.

-u, -U num, -c, -C num
	Create a unified diff (-u, -U) with num lines of context. Create
	a context diff (-c, -C) with num lines of context. The number of
	context lines defaults to 3.

-z[new_name]
	Create a new patch containing the changes instead of refreshing the
	topmost patch. If no new name is specified, \`-2' is added to the
	original patch name, etc. (See the fork command.)

--no-timestamps
	Do not include file timestamps in patch headers.

--no-index
	Do not output Index: lines.

--diffstat
	Add a diffstat section to the patch header, or replace the
	existing diffstat section.

-f	Enforce refreshing of a patch that is not on top.

--backup
	Create a backup copy of the old version of a patch as patch~.

--sort	Sort files by their name instead of preserving the original order.

--strip-trailing-whitespace
	Strip trailing whitespace at the end of lines.
 
Remove patch(es) from the stack of applied patches.  Without options,
the topmost patch is removed.  When a number is specified, remove the
specified number of patches.  When a patch name is specified, remove
patches until the specified patch end up on top of the stack.  Patch
names may include the patches/ prefix, which means that filename
completion can be used.

-a	Remove all applied patches.

-f	Force remove. The state before the patch(es) were applied will
	be restored from backup files.

-R	Always verify if the patch removes cleanly; don't rely on
	timestamp checks.

-q	Quiet operation.

-v	Verbose operation.
 
Remove the specified or topmost patch from the series file.  If the
patch is applied, quilt will attempt to remove it first. (Only the
topmost patch can be removed right now.)

-n	Delete the next patch after topmost, rather than the specified
	or topmost patch.

-r	Remove the deleted patch file from the patches directory as well.

--backup
	Rename the patch file to patch~ rather than deleting it.
	Ignored if not used with \`-r'.
 
Rename the topmost or named patch.

-P patch
	Patch to rename.
 
Take a snapshot of the current working state.  After taking the snapshot,
the tree can be modified in the usual ways, including pushing and
popping patches.  A diff against the tree at the moment of the
snapshot can be generated with \`quilt diff --snapshot'.

-d	Only remove current snapshot.
 
Upgrade the meta-data in a working tree from an old version of quilt to the
current version. This command is only needed when the quilt meta-data format
has changed, and the working tree still contains old-format meta-data. In that
case, quilt will request to run \`quilt upgrade'.
        quilt --version %s: I'm confused.
 Appended text to header of patch %s\n Applied patch %s (forced; needs refresh)\n Applying patch %s\n Can only refresh the topmost patch with -z currently\n Cannot add symbolic link %s\n Cannot diff patches with -p%s, please specify -p0 or -p1 instead\n Cannot refresh patches with -p%s, please specify -p0 or -p1 instead\n Cannot use --strip-trailing-whitespace on a patch that has shadowed files.\n Commands are: Conversion failed\n Converting meta-data to version %s\n Could not determine the envelope sender address. Please use --sender. Delivery address `%s' is invalid
 Diff failed, aborting\n Directory %s exists\n Display name `%s' contains unpaired parentheses
 Failed to back up file %s\n Failed to backup patch file %s\n Failed to copy files to temporary directory\n Failed to create patch %s\n Failed to import patch %s\n Failed to insert patch %s into file series\n Failed to patch temporary files\n Failed to remove patch %s\n Failed to remove patch file %s\n File %s added to patch %s\n File %s exists\n File %s is already in patch %s\n File %s is located below %s\n File %s is not being modified\n File %s is not in patch %s\n File %s may be corrupted\n File %s modified by patch %s\n File series fully applied, ends at patch %s\n Fork of patch %s created as %s\n Fork of patch %s to patch %s failed\n Importing patch %s (stored as %s)\n Importing patch %s\n Interrupted by user; patch %s was not applied.\n Introduction has no subject header (saved as %s)\n Introduction has no subject header\n Introduction saved as %s\n More recent patches modify files in patch %s. Enforce refresh with -f.\n No next patch\n No patch removed\n No patches applied\n No patches in series\n Nothing in patch %s\n Now at patch %s\n Option \`-P' can only be used when importing a single patch\n Options \`--combine', \`--snapshot', and \`-z' cannot be combined.\n Patch %s already exists in series.\n Patch %s appears to be empty, removing\n Patch %s appears to be empty; applied\n Patch %s can be reverse-applied\n Patch %s does not apply (enforce with -f)\n Patch %s does not exist; applied empty patch\n Patch %s does not exist\n Patch %s does not remove cleanly (refresh it or enforce with -f)\n Patch %s exists already, please choose a different name\n Patch %s exists already, please choose a new name\n Patch %s exists already\n Patch %s exists. Replace with -f.\n Patch %s is applied\n Patch %s is currently applied\n Patch %s is not applied\n Patch %s is not in series\n Patch %s is now on top\n Patch %s is unchanged\n Patch %s needs to be refreshed first.\n Patch %s not applied before patch %s\n Patch %s renamed to %s\n Patch headers differ:\n Patches %s have duplicate subject headers.\n Please use -d {o|a|n} to specify which patch header(s) to keep.\n Refreshed patch %s\n Removed patch %s\n Removing patch %s\n Removing trailing whitespace from line %s of %s
 Removing trailing whitespace from lines %s of %s
 Renaming %s to %s: %s
 Renaming of patch %s to %s failed\n Replaced header of patch %s\n Replacing patch %s with new version\n SYNOPSIS: %s [-p num] [-n] [patch]
 The %%prep section of %s failed; results may be incomplete\n The -v option will show rpm's output\n The quilt meta-data in %s are already in the version %s format; nothing to do\n The quilt meta-data in this tree has version %s, but this version of quilt can only handle meta-data formats up to and including version %s. Please pop all the patches using the version of quilt used to push them before downgrading.\n The topmost patch %s needs to be refreshed first.\n The working tree was created by an older version of quilt. Please run 'quilt upgrade'.\n Unable to extract a subject header from %s\n Unpacking archive %s\n Usage: quilt [--trace[=verbose]] [--quiltrc=XX] command [-h] ... Usage: quilt add [-P patch] {file} ...\n Usage: quilt annotate [-P patch] {file}\n Usage: quilt applied [patch]\n Usage: quilt delete [-r] [--backup] [patch|-n]\n Usage: quilt diff [-p n|-p ab] [-u|-U num|-c|-C num] [--combine patch|-z] [-R] [-P patch] [--snapshot] [--diff=utility] [--no-timestamps] [--no-index] [--sort] [--color] [file ...]\n Usage: quilt edit file ...\n Usage: quilt files [-v] [-a] [-l] [--combine patch] [patch]\n Usage: quilt fork [new_name]\n Usage: quilt graph [--all] [--reduce] [--lines[=num]] [--edge-labels=files] [-T ps] [patch]\n Usage: quilt grep [-h|options] {pattern}\n Usage: quilt header [-a|-r|-e] [--backup] [--strip-diffstat] [--strip-trailing-whitespace] [patch]\n Usage: quilt new {patchname}\n Usage: quilt next [patch]\n Usage: quilt patches [-v] {file}\n Usage: quilt pop [-afRqv] [num|patch]\n Usage: quilt previous [patch]\n Usage: quilt refresh [-p n|-p ab] [-u|-U num|-c|-C num] [-z[new_name]] [-f] [--no-timestamps] [--no-index] [--diffstat] [--sort] [--backup] [--strip-trailing-whitespace] [patch]\n Usage: quilt rename [-P patch] new_name\n Usage: quilt series [-v]\n Usage: quilt snapshot [-d]\n Usage: quilt top\n Usage: quilt unapplied [patch]\n Usage: quilt upgrade\n Warning: more recent patches modify files in patch %s\n Warning: trailing whitespace in line %s of %s
 Warning: trailing whitespace in lines %s of %s
 Project-Id-Version: quilt 0.33
PO-Revision-Date: 2007-02-01 14:48+0900
Last-Translator: Yasushi SHOJI <yashi@atmark-techno.com>
Language-Team: Quilt
MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: 8bit
 
æä¸ä½ã¾ãã¯æå®ããããããã«ãã¡ã¤ã«(è¤æ°å¯)ãè¿½å ããããã¡ã¤ã«ã¯ç·¨
éããåã«ããªããè¿½å ããå¿è¦ããããæå®ãããããããä¸ã®ãããã§å¤
æ´ããã¦ãããã¡ã¤ã«ã¯è¿½å ãããã¨ãã§ããªãã

-P ããã
	ãã¡ã¤ã«ãè¿½å ããããã
 
æä¸ä½ã®ãããã«æå®ããããã¡ã¤ã«ãè¿½å å¾ã\$EDITOR (%s) ãä½¿ã£ã¦ç·¨éã
 
æä¸ä½ãããããã©ã¼ã¯ããããããããã©ã¼ã¯ããã¨ã¯åãåå®¹ã®ã³ãã¼ãå¥ã®ååã§ä½æãããã¨ã§ãæ°ããååãåã®æ¹ã«ããã£ã¦ä½¿ç¨ãããããã¯ããããå¤æ´ããªããã°ãªããªãããåã®ãã¼ã¸ã§ã³ãæ®ããªããã°ãªããªãå ´åã«ä¾¿å©ã§ãã(ä¾: å¥ã·ãªã¼ãºã§ã®ä½¿ç¨ãéç¨ã®ä¿å­) ãå¸åçãªã³ãã³ãã®é çªã¨ãã¦ã¯ãforkãeditãrefreshã¨ãªãã

new_nameãæå®ãããªãã£ãå ´åããã©ã¼ã¯ãããããåã¯ç¾å¨ã®ãããåã®å¾ã« \`-2' ãä»ãããã§ã«ãããåã -çªå·ã§çµã£ã¦ããå ´åãçªå·ã®å¤ã1å¢ãã (ä¾: patch.diffãpatch-2.diffãpatch-3.diff)ã
 
dot(1)ãä½¿ã£ã¦é©ç¨ããã¦ãããããã®ä¾å­é¢ä¿ã®ã°ã©ããä½æããããããã
ããä»ã®ãããã«ä¾å­ãã¦ããç¶æã¨ã¯ãä¸¡æ¹ã®ããããåããã¡ã¤ã«ãå¤æ´
ãã¦ããå ´åãå¤æ´ç®æãéãªã£ã(ãªãã·ã§ã³ --linesä½¿ç¨æ)ãæããç¹ã«
ç¤ºãããªãããããã°ã©ãã¯ç¾å¨æä¸ä½ã®ããããä¾å­ããããããã¹ã¦ãå«
ãã

æä¸ä½ãããä»¥å¤ã®ãããåãæå®ãããå ´åã¯ãæå®ããããããã®ã°ã©ã
ãä½æãããã°ã©ãã¯æå®ãããããããä¾å­ãããã¹ã¦ã®ãããã¨ãæå®ã
ãããããã«ä¾å­ãããã¹ã¦ã®ããããå«ãã

--all	é©ç¨ããã¦ãããã¹ã¦ã®ãããã¨ãã®ä¾å­é¢ä¿ã®ã°ã©ããä½æããã
(é©ç¨ããã¦ããªããããã¯å«ã¾ãªã)

--reduce
	Transitive edgeãçç¥ãããtred(1)ãåç§ã

--lines[=è¡æ°]
	ããããå¤æ´ããè¡ããä¾å­é¢ä¿ãè¨ç®ãããè¡æ°ãæå®ãããªãå ´
	åã¯2è¡åã®ã³ã³ãã­ã¹ããä½¿ç¨ãããã

--edge-labels=files
	ã°ã©ãåã®ã¨ãã¸ã«ããããå¤æ´ãããã¡ã¤ã«åãä»ããã

-T ps	ç´æ¥ãã¹ãã¹ã¯ãªãããã¡ã¤ã«ãçæããã
 
å¨ã³ãã³ãå±éãªãã·ã§ã³:

--trace
	ã³ãã³ããbashã®ãã¬ã¼ã¹ã¢ã¼ã(-x)ã§å®è¡ãåé¨ãããã°ç¨ã

--quiltrc file
	~/.quiltrc (å­å¨ããªãå ´åã¯ /etc/quiltrc) ã®ä»£ãã«èª­ã¿è¾¼ã
	ã³ã³ãã£ã®ã¥ã¬ã¼ã·ã§ã³ãã¡ã¤ã«ã®ãæå®ãåå®¹ã®è©³ç´°ã«ã¤ãã¦ã¯
	PDFã®ãã­ã¥ã¡ã³ããåç§ãç¹å¥ãªãã¡ã¤ã«å \"-\"ãä½¿ãã¨ã
	ã³ã³ãã£ã®ã¥ã¬ã¼ã·ã§ã³ãã¡ã¤ã«ãèª­ã¿è¾¼ã¾ãªãã

--version
	ãã¼ã¸ã§ã³æå ±ãåºåãã¦çµäºã 
Quiltã«ãã£ã¦ç®¡çããã¦ãããããã¯ã¡ã¿æå ±ä»¥å¤ã®ã½ã¼ã¹ã³ã¼ããåå¸°ç
ã«grepããããã¡ã¤ã«åãä¸ããããªãã£ãå ´åã¯ãã£ã¬ã¯ããªåãã¹ã¦ãå¯¾
è±¡ã¨ãªãããªãã·ã§ã³ã«ã¤ãã¦ã¯ grep(1)ã®ããã¥ã¢ã«ãã¼ã¸ãåç§ã

-h	ãã®ãã«ããè¡¨ç¤ºãäºéããã·ã¥è¨å· (--)ãä½¿ããã¨ã«ãããgrepã«
-hãæ¸¡ããã¨ãã§ãããããã·ã¥è¨å·ã§å§ã¾ãæ¤ç´¢ãã¿ã¼ã³ã¯2ã¤ç®ã®äºéãã
ã·ã¥è¨å·ã®å¾ã«æ¸¡ããã¨ãã§ããã
 
ç¾å¨ä½æ¥­ä¸­ã®ããªã¼ãä½æãã quiltã¨åããã¼ã¸ã§ã³ã® quiltã§ \`quilt
pop -a'ãå®è¡ãããã¹ã¦ã®ããããã¯ããã¦ãã ãããã¾ãã¯ã%s ãã£ã¬ã¯
ããªãåé¤ããæåãããããããã¦ç´ãã¦ãã ããã\n 
é©ç¨ããã¦ãããããã®ä¸è¦§ãè¡¨ç¤ºãã¾ããæå®ãããããããããå ´åã¯ã
seriesãã¡ã¤ã«åã®ãããä¸è¦§ã®ä¸­ããæå®ããããããã¾ã§ãè¡¨ç¤ºãã¾ãã
 
é©ç¨ããã¦ããªãããããè¡¨ç¤ºãããããæå®ãããå ´åã¯ãæå®ããããã
ãä»¥éã§é©ç¨ããã¦ããªãããããè¡¨ç¤ºã
 
ããããå¤æ´ããè¡ã«è¨»éãä»ãã¦è¡¨ç¤ºãé©å¿ãããã®ã¯ããã¦ããã¦ãããã
ãã®ã¿ã

-P ããã
	æä¸ä½ã®ãããã¾ã§å¦çãè¡ãªãããæå®ããããããã§å¦çãçµäº
 
æä¸ä½ã¾ãã¯æå®ããããããã®ããããåºåã¾ãã¯å¤æ´ããã

-a, -r, -e
	ãããã®ãããã«è¿½å  (-a) ã¾ãã¯ããããå¤æ´ (-r)ã$EDITOR (%s)ã
	ä½¿ã£ã¦ç·¨é (-e)ããããªãã·ã§ã³ãæå®ãããªãã£ãå ´åã¯ãããã®
	ããããåºåããã

--strip-diffstat
	diffstatã®åºåããããããåé¤ããã

--strip-trailing-whitespace
	æ«å°¾ã®ç©ºç½æå­ããããããåé¤ããã

--backup
	å¤ããã¼ã¸ã§ã³ã®ãããã®ããã¯ã¢ããã³ãã¼ãããã~ã¨ãã¦ä½æããã
 
æä¸ä½ããããã¾ãã¯æå®ãããããããå¤æ´ãå ãããã¡ã¤ã«ã®ä¸è¦§ãè¡¨ç¤º
ããã

-a	é©ç¨ããã¦ãããã¹ã¦ã®ããããå¤æ´ãããã¡ã¤ã«ã®ä¸è¦§ãè¡¨ç¤º

-l	ãããåãè¿½å è¡¨ç¤º

-v	è©³ç´°ã§è¦ãããè¡¨ç¤ºã

--combine ããã
	æä¸ä½ãããããæå®ããããããã¾ã§ã®ãããä¸è¦§ãä½æãããã
	åã« \`-' ãæå®ããã¨ãä¸çªæåã«é©ç¨ããã¦ããããããæå®ãã
	ãã¨ã«ãªãã

 
æå®ããããã¡ã¤ã«ã«å¯¾ããå¤æ´ãè¡ãªããããã®ãªã¹ããè¡¨ç¤ºã (é©ç¨ããã¦
ããªãããããæå®ããããã¡ã¤ã«ã«å¯¾ãã¦å¤æ´ãåã¼ããã©ãããèª¿ã¹ãã«ã¯
çºè¦çæ¹æ³ (a heuristic)ãç¨ããããããã®æ¹æ³ã¯é©ç¨ããã¦ãããããã
ã¹ã­ã£ã³ããããå¤§å¹ã«éãã

-v	è©³ç´°ã§è¦ãããè¡¨ç¤ºã
 
æä¸ä½ã¾ãã¯æå®ããããããã®æ¬¡ã®ãããåãè¡¨ç¤ºã
 
æä¸ä½ã®ãããã®åã®ãããåãè¡¨ç¤ºããããåãæå®ãããã¨ãã¯ãæå®
ããããããã®åã®ãããåãè¡¨ç¤ºã
 
ç¾å¨é©ç¨ããã¦ãããããã¹ã¿ãã¯ã®æä¸ä½ãããåãè¡¨ç¤º
 
seriesãã¡ã¤ã«ã«ç»é²ããã¦ããããã¹ã¦ã®ããããè¡¨ç¤ºã

-v	è©³ç´°ã§è¦ãããè¡¨ç¤ºã

 
æä¸ä½ã¾ãã¯æå®ããããããã«ãæå®ããããã¡ã¤ã«ã®å·®åãä½æããã
ãã¡ã¤ã«ãæå®ãããªãã£ãå ´åã¯ãå¤æ´ããããã¡ã¤ã«ãã¹ã¦ãå«ã¾ããã

-p n	-p n ã¹ã¿ã¤ã«ã®ããããä½æ (-p0 ã¾ãã¯ -p1ããµãã¼ãããã¦ãã)

-p ab	-p1ã¹ã¿ã¤ã«ã®ããããä½æããã ãããã©ã«ãã® dir.orig/fileã¨
	dir/fileã¨ããååã§ã¯ãªããa/fileã¨b/fileããããããªãªã¸ãã«
	ã¨æ°ãããã¡ã¤ã«åã¨ããã

-u, -U num, -c, -C num
	numè¡ã®ã³ã³ãã­ã¹ãã§unified diff (-u, -U) ãä½æãnumè¡ã®ã³ã³
	ãã­ã¹ãã§ context diff (-c, -C)ãä½æãããã©ã«ãã®ã³ã³ãã­ã¹
	ãè¡æ°ã¯ 3è¡ã

--no-timestamps
	ããããããã«ã¿ã¤ã ã¹ã¿ã³ããå«ããªãã

--no-index
	Index: ã®è¡ãåºåããªãã

-z	æä¸ä½ãã¾ãã¯æå®ããããããã«é¢é£ããå¤æ´ããæ¨æºåºåã«è¡¨ç¤ºã

-R	reverse diffãä½æã

-P ããã
	æå®ããããããç¨ã« diffãä½æã(ããã©ã«ãã§ã¯æä¸ä½ã®ãããã)

--combine ããã
	ãã®ãããã¨ -Pã§æå®ããããããã®éãã¹ã¦ã®ãããã®å·®åãå
	ãã¦ä¸ã¤ã®ããããä½æã\`-' ã¨ãããããåã¯æåã«é©ç¨ããã
	ããããæå®ããããã®ã¨åãæå³ãããã

--snapshot
	ã¹ãããã·ã§ããã¨ã®å·®åãä½æ (åç§ \`quilt snapshot -h')ã

--diff=utility
	æå®ããã utilityãä½¿ã£ã¦å·®åãçæããªãªã¸ãã«ã®ãã¡ã¤ã«ã¨æ°
	ãããã¡ã¤ã«ãå¼æ°ã¨ãã¦ utilityã«æ¸¡ãããã

--color[=always|auto|never]
	ã·ã³ã¿ãã¯ã¹ã®è²ä»ãæå®ã

--sort	é çªãä¿æãããååé ã«ãã¡ã¤ã«ãä¸¦ã³æããã
 
æå®ãããããããä½æãç´ããããããªãã¬ãã·ã¥ããã¨å¼ã¶ãããã©ã«ã
ã§ã¯æä¸ä½ãããã®ãªãã¬ãã·ã¥ãè¡ãããããã®åé ­ã«æ¸ããã¦ãããã­ã¥
ã¡ã³ãã¯ä¿æãããã

ã¹ã¿ãã¯ã®ä¸çªä¸ä»¥å¤ã®ãããããªãã¬ãã·ã¥ãããã¨ãã§ããããªãã¬ãã·ã¥
ãããããã¨ãã®ä¸ã«ä¹ã£ã¦ãããããããåããã¡ã¤ã«ãå¤æ´ãã¦ããå ´å
ã¯ããã©ã«ãã§ç°å¸¸çµäºããã-fãªãã·ã§ã³ãä»ãããã¨ã§ãªãã¬ãã·ã¥ãå¼·
å¶ãããã¨ãã§ããããå½±ã«ãªã£ããã¡ã¤ã«ãã¨ã« quiltã¯è­¦åãåºããæè¿
ã®ãããã«ããå¤æ´ã¯ç¡è¦ããããããããä»¥å¤ã®å¤æ´ã ããæå®ããããã
ãã«åæ ãããã

-p n	-p n ã¹ã¿ã¤ã«ã®ããããä½æ (-p0 ã¾ãã¯ -p1ããµãã¼ãããã¦ãã)ã

-p ab	-p1ã¹ã¿ã¤ã«ã®ããããä½æããã ãããã©ã«ãã® dir.orig/fileã¨
	dir/fileã¨ããååã§ã¯ãªããa/fileã¨b/fileããããããªãªã¸ãã«
	ã¨æ°ãããã¡ã¤ã«åã¨ããã

-u, -U num, -c, -C num
	numè¡ã®ã³ã³ãã­ã¹ãã§unified diff (-u, -U) ãä½æãnumè¡ã®ã³ã³
	ãã­ã¹ãã§ context diff (-c, -C)ãä½æãããã©ã«ãã®ã³ã³ãã­ã¹
	ãè¡æ°ã¯ 3è¡ã

-z[æ°ããåå]
	ä¸çªä¸ã®ãããããªãã¬ãã·ã¥ããã«ãæ°ããããããä½æãããã
	ãæ°ããååãä¸ããããªãå ´åã¯ã\`-2'ãåã®ãããåã«ä»å ã
	ããã(forkã³ãã³ãåç§)

--no-timestamps
	ããããããã«ã¿ã¤ã ã¹ã¿ã³ããå«ããªãã

--no-index
	Index: ã®è¡ãåºåããªãã

--diffstat
	diffstatã®ã»ã¯ã·ã§ã³ããããã®åé ­é¨åã«è¿½å ãã¾ãã¯æ¢å­ã® 
	diffstatã»ã¯ã·ã§ã³ãä¸æ¸ãããã

-f	ã¹ã¿ãã¯ã®ä¸çªä¸ã«ãªããããã®ãªãã¬ãã·ã¥ãå¼·è¦ããã

--backup
	ããã¯ã¢ããç¨ã®ã³ãã¼ã¨ãã¦å¤ããã¼ã¸ã§ã³ã®ããããããã~ã®
	å½¢ã§ä½æããã

--sort	é çªãä¿æãããååé ã«ãã¡ã¤ã«ãä¸¦ã³æããã

--strip-trailing-whitespace
	æ«å°¾ã®ç©ºç½æå­ãåé¤ããã
 
é©ç¨ããã¦ãããããã¹ã¿ãã¯ããããããã¯ããããªãã·ã§ã³ãç¡ãå ´åã¯
æä¸ä½ããããã¯ãããæ°å­ãæå®ãããã¨ãã¯ãæå®ãããæ°ã®ããããã¯
ããããããåãæå®ãããã¨ãã¯ãæå®ãããããããæä¸ä½ãããã«ãªã
ã¾ã§ããã®ä¸ã«ç©ã¾ãã¦ããããããã¯ããã¦ãããã·ã§ã«ã®ãã¡ã¤ã«åè£å®
æ©è½ãæå¹ã«ä½¿ãããã«ããããåã®åã« patches/ãä»ãã¦æå®ãããã¨ã
å¯è½ã

-a	é©ç¨ããã¦ããããã¹ã¦ã®ããããã¯ããã

-f	åé¡ããããããã§ããå¦çãé²ãããããã¯ã¢ãããã¡ã¤ã«ãä½¿ã£ã¦ã
	ããããé©ç¨ãããåã®ç¶æã«å¾©æ§ããã

-R	å¿ãããããæ­£å¸¸ã«ã¯ããããæ¤è¨¼ãã (ã¿ã¤ã ã¹ã¿ã³ãã®ãã§ãã¯ã«
	ä¾å­ããªã)

-q	è¡¨ç¤ºãæå¶ã

-v	è©³ç´°ã«è¡¨ç¤ºã
 
æå®ãããããããã¾ãã¯æä¸ä½ã®ããããã·ãªã¼ãºãã¡ã¤ã«ããåé¤ããã
ãããããã§ã«é©ç¨ããã¦ããå ´åã¯ãæåã«ããããå¤ãã( ç¾ç¶ãæä¸ä½
ã®ãããããåé¤ãããã¨ã¯ã§ããªãã)

-n	æä¸ä½ããããæå®ããããããã§ã¯ãªããæä¸ä½ãããã®æ¬¡ã®ãããã
	åé¤ããã

-r	ã·ãªã¼ãºãã¡ã¤ã«ããããããæ¶ãã¨ãã«ãpatchesãã£ã¬ã¯ããªããã
	åé¤ããã

--backup
	ããããåé¤ãããããã~ã«ååãå¤æ´ãã¾ãããªãã·ã§ã³\`-r'ã
	æå¹ã§ã¯ãªãã¨ãã¯ç¡è¦ãããã
 
æä¸ä½ã¾ãã¯æå®ããããããã®ååãå¤æ´ãã

-P patch
	ååãå¤æ´ãã patch
 
ç¾å¨ã®ä½æ¥­ç¶æã®ã¹ãããã·ã§ãããä½æãããã¹ãããã·ã§ãããä½æãã
å¾ãããªã¼ã¯ãã¤ãã®ããã«å¤æ´ãããã¨ãå¯è½ããããã pushããã pop
ãããã¨ãã§ãããã¹ãããã·ã§ããã¸ã® diffã¯ã\`quilt diff
--snapshot'ã¨ãããã¨ã§çæã§ããã

-d	ç¾å¨ã®ã¹ãããã·ã§ãããåé¤ã

 
ä½æ¥­ããªã¼ã«ããå¤ããã¼ã¸ã§ã³ã®ã¡ã¿ãã¼ã¿ãæ°ãããã¼ã¸ã§ã³ã«ã¢ããã°
ã¬ã¼ãããããã®ã³ãã³ãã¯ãquiltã®ã¡ã¿ãã¼ã¿ãã©ã¼ããããå¤æ´ããã
å ´åã®ã¿å¿è¦ãå¤æ´ãç¢ºèªãããå ´åãquiltãã \`quilt upgrade'ã®å®è¡ã
è¦æ±ãããã
        quilt --version %s: æ··ä¹±ãã¾ãã
 ããã %s ã®ãããã«è¿½å ãã¾ãã\n ããã %s ãé©ç¨ãã¾ãã (å¼·å¶é©ç¨ããããã«ããªãã¬ãã·ã¥ãå¿è¦ã§ã)\n ããã %s ãé©ç¨ãã¦ãã¾ã\n -zãªãã·ã§ã³ãããå ´åãæä¸ä½ãããä»¥å¤ã®ãªãã¬ãã·ã¥ã¯ã§ãã¾ãã\n ã·ã³ããªãã¯ãã¡ã¤ã« %s ã¯è¿½å ã§ãã¾ãã -p%s ã§ãããã®å·®åã¯ã¨ãã¾ããã-p0ã -p1ãæå®ãã¦ãã ãã\n -p%s ã§ã¯ãªãã¬ãã·ã¥ãããã¨ãã§ãã¾ããã-p0ã¾ãã¯ -p1ãæå®ãã¦ãã ãã\n --strip-trailing-whitespace ã¯å½±ã«ãªã£ã¦ãããã¡ã¤ã«ãããå ´åã¯ä½¿ãã¾ãã ã³ãã³ãä¸è¦§: å¤æ´ã«å¤±æãã¾ãã\n ã¡ã¿ãã¼ã¿ã version %s ã«å¤æ´ä¸­ã§ã\n ã¨ã³ãã­ã¼ãã®éä¿¡èã¢ãã¬ã¹ãç¢ºå®ã§ãã¾ããã§ããã--senderãªãã·ã§ã³
ãä½¿ç¨ãã¦ãã ããã ééåã®ã¢ãã¬ã¹ `%s' ãä¸æ­£ã§ã
 å·®åã«å¤±æãã¾ãããç°å¸¸çµäºãã¾ã\n ãã£ã¬ã¯ããª %s ã¯ããã§ã«å­å¨ãã¾ã\n è¡¨ç¤ºç¨ã®åå `%s' ãå¯¾ã«ãªããªãæ¬å¼§ãå«ãã§ãã¾ã
 ãã¡ã¤ã« %s ã®ããã¯ã¢ããã«å¤±æãã¾ãã\n ãã¡ã¤ã« %s ã®ããã¯ã¢ããã«å¤±æãã¾ãã\n ãã³ãã©ãªãã£ã¬ã¯ããªã¸ã®ãã¡ã¤ã«ã³ãã¼ã«å¤±æãã¾ãã\n ããã %s ã®ä½æã«å¤±æãã¾ãã\n ããã %s ã®åãè¾¼ã¿ã«å¤±æãã¾ãã\n seriesãã¡ã¤ã«ã¸ã®ããã %s ã®æ¸ãè¾¼ã¿ã«å¤±æãã¾ãã\n ãã³ãã©ãªãã¡ã¤ã«ã¸ã®ãããé©ç¨ã«å¤±æãã¾ãã\n ããã %s ã®åé¤ã«å¤±æãã¾ãã\n ããã %s ã®åé¤ã«å¤±æãã¾ãã\n ãã¡ã¤ã« %s ãããã %s ã«è¿½å ãã¾ãã\n ãã¡ã¤ã« %s ã¯ããã§ã«å­å¨ãã¾ã\n ãã¡ã¤ã« %s ã¯ããã§ã«ããã %s ã«å«ã¾ãã¦ãã¾ã\n ãã¡ã¤ã« %s ã¯ã%s ä»¥ä¸ã«ããã¾ã\n ãã¡ã¤ã« %s ã¯å¤æ´ããã¦ãã¾ãã\n ãã¡ã¤ã« %s ã¯ãããã %s ã«å«ã¾ãã¦ãã¾ãã\n ãã¡ã¤ã« %s ã¯å£ãã¦ããå¯è½æ§ãããã¾ã\n ãã¡ã¤ã« %s ã¯ãããã %s ã«ãã£ã¦ãã§ã«å¤æ´ããã¦ãã¾ã\n seriesãã¡ã¤ã«ã®ãããã¯ãã¹ã¦é©ç¨ããã¦ãã¾ããæçµãããã¯ %s ã§ãã\n ããã %s ã®åå²ã§ %s ãä½æããã¾ãã\n ããã %s ããããã %s ã¸ã®åå²ã«å¤±æãã¾ãã\n ããã %s ãåãè¾¼ãã§ãã¾ã (%s ã¨ãã¦ä¿å­ããã¾ã)\n ããã %s ãåãè¾¼ã¿ã¾ã\n ã¦ã¼ã¶ã«ãã£ã¦ä¸­æ­ããã¾ãããããã %s ã¯é©ç¨ããã¦ãã¾ããã\n åºæã«ä»¶åãããã¾ãã(%s ã¨ãã¦ä¿å­ããã¾ãã)\n åºæã«ä»¶åãããã¾ãã\n åºæã¯ %s ã¨ããååã§ä¿å­ããã¾ãã\n ããæè¿ã®ããããããã %s ã®ãã¡ã¤ã«ã«å¤æ´ãå ãã¦ãã¾ãããªãã¬ãã·ã¥ãå®è¡ããå ´åã¯ -f ãªãã·ã§ã³ãä½¿ç¨ãã¦ãã ããã\n æ¬¡ã®ãããã¯ããã¾ãã\n é©ç¨ããã¦ãããããã¯ããã¾ãã\n é©ç¨ããã¦ãããããã¯ããã¾ãã\n ã·ãªã¼ãºã«ç»é²ããã¦ããããããããã¾ãã\n ããã %s ã«ã¯ãªã«ãå«ã¾ãã¦ãã¾ãã\n ç¾å¨ä½ç½®ã¯ããã %s ã§ã\n ãªãã·ã§ã³ \`-P'ã¯ãããããä¸ã¤ã ãåãè¾¼ãã¨ãã®ã¿æå¹ã§ã\n ãªãã·ã§ã³ \`--combine'ã¨ \`--snapshot'ã \`-z'ã¯åæã«ä½¿ãã¾ããã\n ããã %s ã¯ããã§ã« seriesã®ä¸­ã«ããã¾ã\n ããã %s ã¯ãç©ºã®ããã§ããã¯ããã¾ã\n ããã %s ã¯ç©ºã®ããã§ãããé©ç¨ãã¾ãã\n ããã %s ã¯ãåè»¢ãã¦é©ç¨ãããã¨ãã§ãã¾ã\n ããã %s ãé©ç¨ã§ãã¾ãã (å¼·å¶é©ç¨ããå ´åã¯ -fãä»ãã¦ãã ãã)\n ããã %s ã¯å­å¨ãã¾ãããç©ºã®ããããé©ç¨ãã¾ãã\n ããã %s ãå­å¨ãã¾ãã\n ããã %s ããæ­£å¸¸ã«ã¯ãããã¨ãã§ãã¾ãã (ãªãã¬ãã·ã¥ããã -fãä»
ãã¦ã¯ããã¦ãã ãã)\n ããã %s ã¯ãã§ã«å­å¨ãã¾ããæ°ããååãé¸ãã§ãã ãã\n ããã %s ã¯ãã§ã«å­å¨ãã¾ããæ°ããååãé¸ãã§ãã ãã\n ããã %s ã¯ããã§ã«å­å¨ãã¾ã\n ããã %s ã¯ããã§ã«å­å¨ãã¾ãã-fã§ç½®ãæãããã¨ãã§ãã¾ã\n ããã %s ã¯ããã§ã«é©ç¨ããã¦ãã¾ã\n ããã %s ã¯ç¾å¨é©ç¨ããã¦ãã¾ã\n ããã %s ã¯é©ç¨ããã¦ãã¾ãã\n ããã %s ã¯ seriesã®ä¸­ã«ããã¾ãã\n ããã %s ãæä¸ä½ã«ãã¾ãã\n ããã %s ã«å¤æ´ã¯ããã¾ãã\n æåã«ãããã %s ã®ãªãã¬ãã·ã¥ãå¿è¦ã§ãã\n ããã %s ã¯ãããã %s ã®åã«é©ç¨ããã¦ãã¾ãã\n ããã %s ãã %s ã¸ååãå¤æ´ãã¾ãã\n ããããããã®å·®ç°:\n ããã %s ã®ä»¶åãéè¤ãã¦ãã¾ãã\n ã©ã®ãããããããæ®ããããªãã·ã§ã³ -d {o|a|n}ãä½¿ã£ã¦æå®ãã¦ãã ãã ããã %s ããªãã¬ãã·ã¥ãã¾ãã\n ããã %s ãåé¤ãã¾ãã\n ããã %s ãã¯ããã¾ã\n %2$s ã® %1$s è¡ç®æ«å°¾ã«ããç©ºç½æå­ãåé¤ãã¾ã
 %2$s ã® %1$s è¡ç®æ«å°¾ã«ããç©ºç½æå­ãåé¤ãã¾ã
 %s ãã %s ã¸ååãå¤æ´ãã¾ãã: %s
 ããã %s ãã %s ã¸ã®ååã®å¤æ´ã«å¤±æãã¾ãã\n ããã %s ã®ããããå¤æ´ãã¾ãã\n ããã %s ãæ°ãããã¼ã¸ã§ã³ã«ç½®ãæãã¾ã\n æ¸å¼: %s [p num] [-n] [ããã]
 %%prepã»ã¯ã·ã§ã³ã®è§£æã«å¤±æãã¾ãããå®å¨ã«ä½æ¥­ãå®äºãã¦ããªãå ´åãããã¾ã\n -vãªãã·ã§ã³ãä½¿ã£ã¦ãrpmã®åºåãè¡¨ç¤ºã§ãã¾ã %s åã® quilt ã¡ã¿ãã¼ã¿ã¯ãã§ã« version %s ãã©ã¼ãããã®ãããå¦çããå¿è¦ãããã¾ããã\n ãã®ããªã¼åã«ã¯ãã¼ã¸ã§ã³ %sã® quiltã¡ã¿ãã¼ã¿ãå­å¨ãã¾ãããããã
ã®ãã¼ã¸ã§ã³ã® quiltã§ã¯ããã¼ã¸ã§ã³ %s ã¾ã§ã®ã¡ã¿ãã¼ã¿ãããµãã¼ãã
ã¦ãã¾ããããã¦ã³ã°ã¬ã¼ãããåã«ãpushãããã¼ã¸ã§ã³ã® quiltãä½¿ã£ã¦ã
ãã¹ã¦ã®ãããã pop ãã¦ãã ããã\n æä¸ä½ãããã®ãªãã¬ãã·ã¥ãæåã«å¿è¦ã§ãã\n ç¾å¨ä½æ¥­ä¸­ã®ãã£ã¬ã¯ããªã¯å¤ããã¼ã¸ã§ã³ã® quiltã«ãã£ã¦ä½ããããã®ã§ãã'quilt upgrade'ãå®è¡ãã¦ãã ããã\n %s ããä»¶åãåãåºãã¾ããã\n ã¢ã¼ã«ã¤ã %s ãå±éãã¦ãã¾ã\n ä½¿ãæ¹: quilt [--trace[=verbose]] [--quiltrc=XX] command [-h] ... ä½¿ãæ¹: quilt add [-P ããã] {file} ...\n ä½¿ãæ¹: quilt annotate [-P ããã] {file}\n ä½¿ãæ¹: quilt applied [ããã]\n ä½¿ãæ¹: quilt delete [-r] [--backup] [ããã|-n]\n ä½¿ãæ¹: quilt diff [-p n|-p ab] [-u|-U æ°|-c|-C æ°] [--combine ããã|-z] [-R] [-P ããã] [--snapshot] [--diff=utility] [--no-timestamps] [--no-index] [--sort] [--color] [ãã¡ã¤ã« ...]\n ä½¿ãæ¹: quilt edit ãã¡ã¤ã« ...\n ä½¿ãæ¹: quilt files [-v] [-a] [-l] [--combine ããã] [ããã]\n ä½¿ãæ¹: quilt fork [new_name]\n ä½¿ãæ¹: quilt graph [--all] [--reduce] [--lines[=num]] [--edge-labels=files] [-T ps] [ããã]\n ä½¿ãæ¹: quilt grep [-h|options] {ãã¿ã¼ã³}\n ä½¿ãæ¹: quilt header [-a|-r|-e] [--backup] [--strip-diffstat] [--strip-trailing-whitespace] [ããã]\n ä½¿ãæ¹: quilt new {ãããå}\n ä½¿ãæ¹: quilt next [ããã]\n ä½¿ãæ¹: quilt patches [-v] {ãã¡ã¤ã«}\n ä½¿ãæ¹: quilt pop [-afRqv] [æ°å­|ããã]\n ä½¿ãæ¹: quilt previous [ããã]\n ä½¿ãæ¹: quilt refresh [-p n|-p ab] [-u|-U num|-c|-C num] [-z[æ°ããåå]] [-f] [--no-timestamps] [--no-index] [--diffstat] [--sort] [--backup] [--strip-trailing-whitespace] [ããã]\n ä½¿ãæ¹: quilt rename [-P patch] new_name\n ä½¿ãæ¹: quilt series [-v]\n ä½¿ãæ¹: quilt snapshot [-d]\n ä½¿ãæ¹: quilt top\n ä½¿ãæ¹: quilt unapplied [ããã]\n ä½¿ãæ¹: quilt upgrade\n è­¦å: æè¿ã®ãããããããã %s åã®ãã¡ã¤ã«ãå¤æ´ãã¦ãã¾ã\n è­¦å: %2$s ã® %1$s è¡ç®æ«å°¾ã«ç©ºç½æå­ãããã¾ã
 è­¦å: %2$s ã® %1$s è¡ç®æ«å°¾ã«ç©ºç½æå­ãããã¾ã
 