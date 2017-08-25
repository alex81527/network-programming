port=$1
a=$2
index=1
echo "exec test1.txt"
./client nplinux3.cs.nctu.edu.tw $port test/test1.txt > $a$index
index=`expr $index + 1`
echo "exec test2.txt"
./client nplinux3.cs.nctu.edu.tw $port test/test2.txt > $a$index
index=`expr $index + 1`
echo "exec test3.txt"
./client nplinux3.cs.nctu.edu.tw $port test/test3.txt > $a$index
index=`expr $index + 1`
echo "exec test4.txt"
./client nplinux3.cs.nctu.edu.tw $port test/test4.txt > $a$index
index=`expr $index + 1`
echo "exec test5.txt"
./client nplinux3.cs.nctu.edu.tw $port test/test5.txt > $a$index
index=`expr $index + 1`
echo "exec test6.txt"
./client nplinux3.cs.nctu.edu.tw $port test/test6.txt > $a$index
index=`expr $index + 1`
echo "exec test7.txt"
./client nplinux3.cs.nctu.edu.tw $port test/test7.txt > $a$index
index=`expr $index + 1`
