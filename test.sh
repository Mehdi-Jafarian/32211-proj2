#!/bin/bash -e
echo "Compiling and Buidling the MMU"   
make clean
make mmu     

echo ""
echo "--------------- Phase 1 ---------------------"
echo "Running Phase 1"
./mmu 256 BACKING_STORE.bin addresses.txt


echo "Grading Phase 1 ..."

DIFF=$(diff -wB output256.csv correct256.csv || true) 
if [ "$DIFF" != "" ] 
then
	echo "Something is wrong."
	echo $DIFF
else
	echo "Good Job -- 8 points"
fi


echo ""
echo "--------------- Phase 2 ---------------------"
echo "Running Phase 2"

./mmu 128 BACKING_STORE.bin addresses.txt


echo "Grading Phase 2 ..."
DIFF=$(diff -wB output128.csv correct128.csv || true) 
if [ "$DIFF" != "" ] 
then
	echo "Something is wrong."
	echo $DIFF
else
	echo "Good Job -- 12 points"
fi

echo ""