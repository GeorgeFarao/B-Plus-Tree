main1:
	@echo " Compile main1 ...";
	gcc -g -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/main1.c ./src/AM.c -lbf -o ./build/main1

main2:
	@echo " Compile main2 ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/main2.c ./src/AM.c -lbf -o ./build/main2

main3:
	@echo " Compile main3 ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/main3.c ./src/AM.c -lbf -o ./build/main3
bf:
	@echo " Compile bf_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/bf_main.c -lbf -o ./build/runner -O2
