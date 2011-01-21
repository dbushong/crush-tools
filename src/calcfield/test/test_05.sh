test_number=05
description="whitespace at line edges"

infile="$test_dir/edge_space.txt"
outfile="$test_dir/test_$test_number.actual"
expected="$test_dir/test_$test_number.expected"

$bin -d '\t' -e '[bar]+[baz]' -c tot $infile > "$outfile"

if [ $? -ne 0 ] ||
   [ "`diff -q $outfile $expected`" ]; then
  test_status $test_number 1 "$description" FAIL
else
  test_status $test_number 1 "$description" PASS
  rm "$outfile"
fi
