autoreconf -fvi
./configure --prefix=/usr

echo
echo "Source code configured. Please compile it with"
echo "$ make -j`nproc`"
