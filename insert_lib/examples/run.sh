for i in 1 2 4 8 10; do
  for j in 100 1000 10000 40000 100000 200000; do
    ./test $i $j 100 | grep "DATA: " | sed 's/^DATA: //' >> data.log
  done
done
