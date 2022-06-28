#!/usr/bin/env python
import os
from tempfile import TemporaryDirectory
from subprocess import check_output, check_call

from sys import argv, stderr, exit
import hashlib


def local_test():
    out = check_output([test_bin + "/testLocal", test_bw])
    md5sum = hashlib.md5(out).hexdigest()
    assert md5sum == "1c52065211fdc44eea45751a9cbfffe0"


def remote_http_test():
    if not os.path.exists(test_bin + "/testRemote"):
        print("libBigWig was compiled without CURL. Skipping test with testRemote!", file=stderr)
        return

    out = check_output([test_bin + "/testRemote",
                        "http://hgdownload.cse.ucsc.edu/goldenPath/hg19/encodeDCC/wgEncodeMapability/wgEncodeCrgMapabilityAlign50mer.bigWig"])
    md5sum = hashlib.md5(out).hexdigest()
    assert md5sum == "9ccecd6c32ff31042714c1da3c0d0eba"


def test_recreating_file():
    with TemporaryDirectory(prefix="libbigwig-test") as tmpdir:
        tmpout = os.path.join(tmpdir, "output.bw")
        p1 = check_call([test_bin + "/testWrite", test_bw, tmpout])
        assert p1 == 0
        with open(tmpout, mode="rb") as f:
            md5sum = hashlib.md5(f.read()).hexdigest()
            assert md5sum == "8e116bd114ffd2eb625011d451329c03"


def test_creation_from_scratch():
    with TemporaryDirectory(prefix="libbigwig-test") as tmpdir:
        tmpout = os.path.join(tmpdir, "test", "example_output.bw")
        os.mkdir(os.path.dirname(tmpout))
        p1 = check_call([test_bin + "/exampleWrite"], cwd=tmpdir)
        assert p1 == 0
        with open(tmpout, mode="rb") as f:
            md5sum = hashlib.md5(f.read()).hexdigest()
            assert md5sum == "ef104f198c6ce8310acc149d0377fc16"


def remote_test2():
    ## Ensure that we can properly parse chromosome trees with non-leaf nodes
    # The UCSC FTP site is timing out for OSX!
    if not os.path.exists(test_bin + "/testRemoteManyContigs"):
        print("libBigWig was compiled without CURL. Skipping test with testRemoteManyContigs!", file=stderr)
        return

    out = check_output([test_bin + "/testRemoteManyContigs",
                        "http://hgdownload.cse.ucsc.edu/gbdb/dm6/bbi/gc5BaseBw/gc5Base.bw"])

    md5sum = hashlib.md5(out).hexdigest()
    assert md5sum == "a15a3120c03ba44a81b025ebd411966c"


def test_bigbed():
    if not os.path.exists(test_bin + "/testBigBed"):
        print("libBigWig was compiled without CURL. Skipping test with testBigBed!", file=stderr)
        return

    out = check_output([test_bin + "/testBigBed",
                        "https://www.encodeproject.org/files/ENCFF001JBR/@@download/ENCFF001JBR.bigBed"])

    md5sum = hashlib.md5(out).hexdigest()
    assert md5sum == "33ef99571bdaa8c9130149e99332b17b"


if __name__ == "__main__":
    if len(argv) != 3:
        print("Usage: {} path/to/test/bin path/to/test.bw".format(argv[0]), file=stderr)
        print("Example: {} build/test/ test/test.bw".format(argv[0]), file=stderr)
        exit(1)

    test_bin = os.path.abspath(argv[1])
    test_bw = os.path.abspath(argv[2])

    local_test()
    remote_http_test()
    test_recreating_file()
    test_creation_from_scratch()
    remote_test2()
    test_bigbed()

    print("success", file=stderr)
