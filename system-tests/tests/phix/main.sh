mpirun -tag-output -np $NSLOTS $RAY_GIT_PATH/Ray \
-p 1.fasta \
   2.fasta \
-o $TEST_NAME

ValidateGenomeAssembly.sh phix.fasta $TEST_NAME.Contigs.fasta $TEST_NAME.Ray
