#!/usr/bin/perl
# create hldump.h from hotline.h
$hlh="../common/hotline.h";
open(FILE, $hlh) or die "Cannot find hotline.h, bye";
# hl header types
print "\
struct hl_hdr_type {\
	char *name;\
	u_int32_t type;\
};\
\
struct hl_hdr_type hl_h_types[] = {\n";
while (<FILE>) {
	if (m/HTL._HDR/) {
		@l = split(" ",$_);
		$n=$l[1];
		$v=$l[3];
		$v=~s/\)//;
		print "	{ \"$n\", $v },\n";
	}
}
print "};\n";
# hl data header types
seek(FILE, 0, SEEK_SET);
print "\
struct hl_data_hdr_type {\
	char *name;\
	u_int16_t type;\
};\
\
struct hl_data_hdr_type hl_dh_types[] = {\n";
while (<FILE>) {
	if (m/HTL._DATA/) {
		@l = split(" ",$_);
		$n=$l[1];
		$v=$l[3];
		$v=~s/\)//;
		print "	{ \"$n\", $v },\n";
	}
}
print "};\n";
close FILE;
