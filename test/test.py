#!/usr/bin/env python
from subprocess import Popen, PIPE, check_call
from os import remove

# N.B., this MUST be run from within the source directory!

# local test
p1 = Popen(["./test/testLocal", "test/test.bw"], stdout=PIPE)
try:
    p2 = Popen(["md5sum"], stdin=p1.stdout, stdout=PIPE)
except:
    p2 = Popen(["md5"], stdin=p1.stdout, stdout=PIPE)
md5sum = p2.communicate()[0].strip().split()[0]
assert(md5sum == "a15cbf0021f3e80d9ddfd9dbe78057cf")

# remote http test
p1 = Popen(["./test/testRemote", "http://hgdownload.cse.ucsc.edu/goldenPath/hg19/encodeDCC/wgEncodeMapability/wgEncodeCrgMapabilityAlign50mer.bigWig"], stdout=PIPE)
try:
    p2 = Popen(["md5sum"], stdin=p1.stdout, stdout=PIPE)
except:
    p2 = Popen(["md5"], stdin=p1.stdout, stdout=PIPE)
md5sum = p2.communicate()[0].strip().split()[0]
assert(md5sum == "9ccecd6c32ff31042714c1da3c0d0eba")

# test recreating a file
p1 = check_call(["./test/testWrite", "test/test.bw", "test/output.bw"])
assert(p1 == 0)
try:
    p2 = Popen(["md5sum", "test/output.bw"], stdout=PIPE)
except:
    p2 = Popen(["md5", "test/output.bw"], stdout=PIPE)
md5sum = p2.communicate()[0].strip()
md5sumuse = md5sum.split()[0]
try:
    assert(md5sumuse == "8e116bd114ffd2eb625011d451329c03")
except:
    md5sum = md5sum.split(" ")[-1]
    assert(md5sum == "8e116bd114ffd2eb625011d451329c03")
remove("test/output.bw")

# test creation from scratch with multiple interval types
p1 = check_call(["./test/exampleWrite"])
assert(p1 == 0)
try:
    p2 = Popen(["md5sum", "test/example_output.bw"], stdout=PIPE)
except:
    p2 = Popen(["md5", "test/example_output.bw"], stdout=PIPE)
md5sum = p2.communicate()[0].strip()
md5sumuse = md5sum.split()[0]
try:
    assert(md5sumuse == "ef104f198c6ce8310acc149d0377fc16")
except:
    md5sum = md5sum.split(" ")[-1]
    assert(md5sum == "ef104f198c6ce8310acc149d0377fc16")
remove("test/example_output.bw")

## Ensure that we can properly parse chromosome trees with non-leaf nodes
# The UCSC FTP site is timing out for OSX!
p1 = Popen(["./test/testRemoteManyContigs", "http://hgdownload.cse.ucsc.edu/gbdb/dm6/bbi/gc5BaseBw/gc5Base.bw"], stdout=PIPE)
try:
    p2 = Popen(["md5sum"], stdin=p1.stdout, stdout=PIPE)
except:
    p2 = Popen(["md5"], stdin=p1.stdout, stdout=PIPE)
md5sum = p2.communicate()[0].strip().split()[0]
assert(md5sum == "a15a3120c03ba44a81b025ebd411966c")

# Try a bigBed file
p1 = Popen(["./test/testBigBed", "https://www.encodeproject.org/files/ENCFF001JBR/@@download/ENCFF001JBR.bigBed"], stdout=PIPE)
try:
    p2 = Popen(["md5sum"], stdin=p1.stdout, stdout=PIPE)
except:
    p2 = Popen(["md5"], stdin=p1.stdout, stdout=PIPE)
md5sum = p2.communicate()[0].strip().split()[0]
assert(md5sum == "33ef99571bdaa8c9130149e99332b17b")
