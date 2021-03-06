#!/usr/bin/perl
# cgi-bin/ctsim.cgi.  Generated from ctsim.cgi.in by configure.

use strict;
use CGI;
use File::Basename;
use Fcntl ':flock';

require './ctsim.conf';

my $opt_d = 0;
$ENV{'PATH'} = "$::bindir:$::lamrundir:/bin";

my $fromhost = $ENV{'REMOTE_HOST'};

my $gmt_start = time();
my $gmt_end;
my $execution_time;

my %in;
CGI::ReadParse(\%in);

# Incoming variables 
#   Phantom_Name, Phantom_Nx, Phantom_Ny, Phantom_NSample
#   PJ_NDet, PJ_NRot, PJ_NRay, PJ_RotAngle,
#   IR_Nx, IR_Ny, IR_Filter, IR_Filter_Param

my $error = "";

if ($::single_process_only > 0) {
  my @processes = `/bin/ps -A`;
  my $running = 0;
  foreach my $p (@processes) {
    if ($p =~ m/ctsimtext/) {
      $error .= "Another online simulation is currently running.<br>You may wait a moment and then click your browser's <b>Refresh</b> button to resubmit your simulation<br>";
      last;
    }
  }
}

my $Phantom_Name = FilterMetaChars($in{'Phantom_Name'});
my $Phantom_Nx = FilterToNumber($in{'Phantom_Nx'});
my $Phantom_Ny = FilterToNumber($in{'Phantom_Ny'});
my $Phantom_ViewRatio = FilterToNumber($in{'Phantom_ViewRatio'});
my $Phantom_NSample = FilterToNumber($in{'Phantom_NSample'});
$error .= "Phantom name must not be blank<br>" if ($Phantom_Name eq "");
$error .= "Phantom Size must be between 5 and 512<br>" if ($Phantom_Nx < 5 || $Phantom_Nx > 512 || $Phantom_Ny < 5 || $Phantom_Ny > 512);
$error .= "Phantom NSample must be between 1 and 5<br>" if ($Phantom_NSample < 1 || $Phantom_NSample > 5);
$error .= "View Ratio must be between 0 and 100<br>" if ($Phantom_ViewRatio <= 0 || $Phantom_ViewRatio > 100);

my $PJ_Geometry = FilterMetaChars($in{'PJ_Geometry'});
my $PJ_NDet = FilterToNumber($in{'PJ_NDet'});
my $PJ_NRot = FilterToNumber($in{'PJ_NRot'});
my $PJ_FocalLength = FilterToNumber($in{'PJ_FocalLength'});
my $PJ_NRay = FilterToNumber($in{'PJ_NRay'});
my $PJ_RotAngle = FilterToNumber($in{'PJ_RotAngle'});
my $PJ_ScanRatio = FilterToNumber($in{'PJ_ScanRatio'});
$error .= "Projection NDet must be between 5 and 1000<br>" if ($PJ_NDet < 5 || $PJ_NDet > 1000);
$error .= "Projection NRot must be between 5 and 1000<br>" if ($PJ_NRot < 5 || $PJ_NRot > 1000);
$error .= "Projection NRay must be between 1 and 5<br>" if ($PJ_NRay < 1 || $PJ_NRay > 5);
$error .= "Projection RotAngle must be between 0.1 and 2<br>" if ($PJ_RotAngle < 0.1 || $PJ_RotAngle > 2);
$error .= "Focal length must be between 1.01 and 10<b>" if ($PJ_FocalLength <= 1 || $PJ_FocalLength > 10);
$error .= "Scan Ratio must be between 0 and 10<br>" if ($PJ_ScanRatio <= 0 || $PJ_ScanRatio > 10);

#my $IR_Nx = FilterToNumber($in{'IR_Nx'});
#my $IR_Ny = FilterToNumber($in{'IR_Ny'});
my $IR_Nx = $Phantom_Nx;
my $IR_Ny = $Phantom_Ny;
my $IR_FilterMethod = FilterMetaChars($in{'IR_FilterMethod'});
my $IR_Zeropad = FilterToNumber($in{'IR_Zeropad'});
my $IR_Filter = FilterMetaChars($in{'IR_Filter'});
my $IR_Filter_Param = FilterToNumber($in{'IR_Filter_Param'});
my $IR_Interp = FilterMetaChars($in{'IR_Interp'});
my $IR_Backproj = FilterMetaChars($in{'IR_Backproj'});

my $Disp_Min = "auto";
my $Disp_Max = "auto";
$Disp_Min = FilterToNumber($in{'Disp_Min'}) if ($in{'Disp_Min'} ne "auto" && $in{'Disp_Min'} ne "");
$Disp_Max = FilterToNumber($in{'Disp_Max'}) if ($in{'Disp_Max'} ne "auto" && $in{'Disp_Max'} ne "");
if ($Disp_Min ne 'auto' && ! ($Disp_Min =~ /^[\d\.\-]+$/)) {
    $error .= "Display Minimum must be 'auto' or numeric (received $Disp_Min) <br>";
}
if ($Disp_Max ne 'auto' && ! ($Disp_Max =~ /^[\d\.\-]+$/)) {
    $error .= "Display Maximum must be 'auto' or numeric (received $Disp_Max) <br>";
}

my $MPI_Str = FilterMetaChars($in{'MPI'});
my $MPI = 0;
$MPI = 1 if ($MPI_Str eq "yes" && $::mpi_enable ne "");

$error .= "IR Nx and Ny must be between 5 and 1024<br>" if ($IR_Nx < 5 || $IR_Nx > 1024 || $IR_Ny < 5 || $IR_Ny > 1024);
$error .= "IR Filter Parameter must be between 0 and 1<br>" if ($IR_Filter_Param < 0 || $IR_Filter_Param > 1);


my $tmpid = $$;
my $auto_window_img = "std0.1";
my $auto_window_diff = "std1";
my $auto_window_pj = "full";
my $logfile = "$::jobdir/ctsim.log";

my $result_fname = "$::datadir/result-$tmpid.html";
my $phantom_fname = "$::datadir/phantom-$tmpid.if";
my $pj_fname = "$::datadir/pj-$tmpid.pj";
my $ir_fname = "$::datadir/ir-$tmpid.if";
my $pj_if_fname = "$::datadir/pj-$tmpid.if";
my $sub_fname = "$::datadir/sub-$tmpid.if";
my $phantom_png = "$::datadir/phantom-$tmpid.png";
my $ir_png = "$::datadir/ir-$tmpid.png";
my $pj_png = "$::datadir/pj-$tmpid.png";
my $sub_png = "$::datadir/sub-$tmpid.png";

my $result_url = "$::url_datadir/result-$tmpid.html";
my $phantom_png_url = "$::url_datadir/phantom-$tmpid.png";
my $ir_png_url = "$::url_datadir/ir-$tmpid.png";
my $pj_png_url = "$::url_datadir/pj-$tmpid.png";
my $sub_png_url = "$::url_datadir/sub-$tmpid.png";

my $pjrec_ver = "$::bindir/ctsimtext pjrec";
my $phm2pj_ver = "$::bindir/ctsimtext phm2pj";
my $phm2if_ver = "$::bindir/ctsimtext phm2if";
my $diff_ver = "$::bindir/ctsimtext if2";

$pjrec_ver = "/opt/lam/bin/mpirun N N $::lamrundir/pjrec-lam" if $MPI;
$phm2pj_ver = "/opt/lam/bin/mpirun N N $::lamrundir/phm2pj-lam" if $MPI;
$phm2if_ver = "/opt/lam/bin/mpirun N N $::lamrundir/phm2if-lam" if $MPI;

my $gp_cmd = "$phm2if_ver $phantom_fname $Phantom_Nx $Phantom_Ny --phantom $Phantom_Name --nsample $Phantom_NSample --view-ratio $Phantom_ViewRatio";
my $pj_cmd = "$phm2pj_ver $pj_fname $PJ_NDet $PJ_NRot --phantom $Phantom_Name --nray $PJ_NRay --rotangle $PJ_RotAngle --geometry $PJ_Geometry --focal-length $PJ_FocalLength --view-ratio $Phantom_ViewRatio --scan-ratio $PJ_ScanRatio";
my $pj_if_cmd = "$::bindir/pj2if $pj_fname $pj_if_fname";
my $pjrec_cmd = "$pjrec_ver $pj_fname $ir_fname $IR_Nx $IR_Ny --filter $IR_Filter --filter-param $IR_Filter_Param --interp $IR_Interp --backproj $IR_Backproj --filter-method $IR_FilterMethod --zeropad $IR_Zeropad";
my $sub_cmd = "$diff_ver $phantom_fname $ir_fname $sub_fname --sub";
my $diff_cmd = "$diff_ver $phantom_fname $ir_fname --comp";

my $window_options = "--auto $auto_window_img";
if ($Disp_Min ne 'auto') {
    $window_options .= " --min $Disp_Min";
}
if ($Disp_Max ne 'auto') {
    $window_options .= " --max $Disp_Max";
}

my $png1_cmd = "$::bindir/ifexport $phantom_fname $phantom_png $window_options --stats --format png --center mode";
my $png2_cmd = "$::bindir/ifexport $ir_fname $ir_png $window_options --stats --format png --center mode";
my $png3_cmd = "$::bindir/ifexport $pj_if_fname $pj_png --auto $auto_window_pj --stats --format png --center mode";
my $png4_cmd = "$::bindir/ifexport $sub_fname $sub_png --auto $auto_window_diff --stats --format png --center mode";

my $title = "CTSim Results";

my $out = head();
$out .= "<HTML> <HEAD><TITLE> $title </TITLE><LINK rel=\"stylesheet\" href=\"http://www.ctsim.org/main.css\" type=\"text/css\">
</HEAD>\n";
$out .= "<BODY  BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\"  VLINK=\"#800020\" LINK=\"#0000FF\">\n";
$out .= "<H1>$title</H1><HR>\n";

if ($opt_d) {
    $out .= "<H2>Commands</H2>\n";
    $out .= "$gp_cmd<br>\n$pj_cmd<br>\n$pj_if_cmd<br>\n$pjrec_cmd<br>\n$diff_cmd<br>\n$png1_cmd<br>\n$png2_cmd<br>\n" .
            "$png3_cmd<br>\n$png4_cmd<br>\n";
}

my $cmdout = "";
if ($error ne "") {
    $out .= "<FONT COLOR=\"#FF0000\">The following errors were present in your request.<br>\n";
    $out .= "Please correct them and try submitting your request again.</FONT><br>\n";
    $out .= "<i>$error</i>";
} else {
  my $gp_out;
  my $pj_out;
  my $pj_if_out;
  my $pjrec_out;
  my $sub_out;
  my $diff_out;
  my $png_gp_out;
  my $png_pjrec_out;
  my $png_pj_out;
  my $png_sub_out;
  $gp_out = `$gp_cmd`;
  if (-s $phantom_fname) {
    $pj_out .= `$pj_cmd`;
    $png_gp_out .= `$png1_cmd`;
    if (-s $pj_fname) {
      $pj_if_out .= `$pj_if_cmd`;
      $png_pj_out .= `$png3_cmd`;
      $pjrec_out .= `$pjrec_cmd`;
      if (-s $ir_fname) {
	$png_pjrec_out .= `$png2_cmd`;
	$sub_out .= `$sub_cmd`;
	$diff_out .= `$diff_cmd`;
	$png_sub_out .= `$png4_cmd`;
      }
    }
  }
  # Delete image files and projection files
  unlink($phantom_fname);
  unlink($ir_fname);
  unlink($pj_fname);
  unlink($pj_if_fname);
  unlink($sub_fname);

  $cmdout = "$gp_cmd\n $gp_out $pj_cmd\n $pj_out $pj_if_cmd\n $pj_if_out $pjrec_cmd\n $pjrec_out $diff_cmd\n $diff_out $png1_cmd\n $png_gp_out $png2_cmd\n $png_pjrec_out $png3_cmd\n $png_pj_out $png4_cmd\n $png_sub_out";
  if (open(LOGFILE,">> $logfile")) {
    flock(LOGFILE,LOCK_EX);
    seek(LOGFILE, 0, 2);
    print LOGFILE "Client Address: $ENV{'REMOTE_ADDR'}\n";
    print LOGFILE "Job $tmpid\n";
    print LOGFILE $cmdout;
    print LOGFILE "----------------------------------------------------\n";
    flock(LOGFILE,LOCK_UN);
    close(LOGFILE);
  }
  $gmt_end = time();
  $execution_time = $gmt_end - $gmt_start;
  if ($opt_d) {
    $out .= "<H3>Command Output</H3>$cmdout<HR>\n";
  }
  my $png_gp_out_html = $png_gp_out;
  my $png_pjrec_out_html = $png_pjrec_out;
  my $png_pj_out_html = $png_pj_out;
  my $png_sub_out_html = $png_sub_out;
  $png_gp_out_html =~ s/\n/<br>/gms;
  $png_pjrec_out_html =~ s/\n/<br>/gms;
  $png_pj_out_html =~ s/\n/<br>/gms;
  $png_sub_out_html =~ s/\n/<br>/gms;
  $out .= "<TABLE><TR><TD><b>Phantom Image</b></TD><TD><b>Reconstructed Image</b></TD></TR>\n";
  $out .= "<TR><TD><IMG SRC=\"$phantom_png_url\"><br><FONT SIZE=1>$png_gp_out</FONT></TD>\n";
  $out .= "<TD><IMG SRC=\"$ir_png_url\"><br><FONT SIZE=1>$png_pjrec_out</FONT></TD></TR>\n";
  $out .= "<TR><TD><b>Projection Sinogram</b></TD><TD><b>Reconstruction Error</b></TD></TR>\n";
  $out .= "<TR><TD><IMG SRC=\"$pj_png_url\"><br><FONT SIZE=1>$png_pj_out</FONT></TD>\n";
  $out .= "<TD><IMG SRC=\"$sub_png_url\"><br><FONT SIZE=2>$sub_out</FONT><br><FONT SIZE=1>$png_sub_out</FONT></TD></TR>\n";
  $out .= "</TABLE>";
  $out .= "<p><b>Error Measurements</b><br>";
  $out .= "$diff_out";
  $out .= "<p>Execution time: $execution_time seconds\n";
}

$out .= "<HR>\n";
$out .= "Specify another <a href=\"http://www.ctsim.org/simulate.shtml\">simulation</a>.<br>";
$out .= "Return to CTSim's <A HREF=\"http://www.ctsim.org\">home</a>.<br>\n";
$out .= "</BODY> </HTML>";
$out .= "\n";

if (open(OUTFILE,"> $result_fname"))
{
    flock(OUTFILE,LOCK_EX);
    print OUTFILE $out;
    flock(OUTFILE,LOCK_UN);
    close OUTFILE;
    print "Location: $result_url\n\n";
}
else
{
    print "Content-type: text/plain\n\n";
    print "The simulator was unable to create an result file.\n";
}
if (open(JOBFILES,"> $::jobdir/$tmpid"))
{
    flock(JOBFILES,LOCK_EX);
    print JOBFILES "execution_time=$execution_time\n";
    print JOBFILES "Phantom_Name=$Phantom_Name\n";
    print JOBFILES "Phantom_Nx=$Phantom_Nx\n";
    print JOBFILES "Phantom_Ny=$Phantom_Nx\n";
    print JOBFILES "Phantom_NSample=$Phantom_NSample\n";
    print JOBFILES "PJ_NDet=$PJ_NDet\n";
    print JOBFILES "PJ_NRot=$PJ_NRot\n";
    print JOBFILES "PJ_NRay=$PJ_NRay\n";
    print JOBFILES "PJ_RotAngle=$PJ_RotAngle\n";
    print JOBFILES "IR_Nx=$IR_Nx\n";
    print JOBFILES "IR_Ny=$IR_Ny\n";
    print JOBFILES "IR_Interp=$IR_Interp\n";
    print JOBFILES "IR_Filter=$IR_Filter\n";
    print JOBFILES "IR_Filter_Param=$IR_Filter_Param\n";
    print JOBFILES "IR_Backproj=$IR_Backproj\n";
    print JOBFILES "Disp_Min=$Disp_Min\n";
    print JOBFILES "Disp_Max=$Disp_Max\n";
    print JOBFILES "MPI=$MPI\n";
    print JOBFILES "Files=$result_fname,$phantom_fname,$pj_fname,$ir_fname,$phantom_png,$ir_png,$pj_if_fname,$pj_png\n" if ($error eq "");
    printf JOBFILES "cmdout=$cmdout\n";
    flock(JOBFILES,LOCK_UN);
    close JOBFILES;
}


exit;


sub internal_error
{
  my $out = head();
  $out .= "<H1>Internal error</H1>
  Please notify <A HREF=mailto:webmaster\@med-info.com>webmaster\@med-info.com</A>
  </BODY>";
  print $out;
  exit;
}

sub head
{
#  "Content: text/html\n\n";
}


sub FilterMetaChars
{
   my $var = pop(@_);
   $var =~ /^([-\w]+)$/;  
   $1;
}

sub FilterToNumber
{
   my $var = pop(@_);
   $var =~ /^(\-*\d*\.*\d*)$/;  
   $1;
}
