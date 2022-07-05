all: medusalog64 medusalog

medusalog64: medusalog64.cpp
	clang++ -pthread -o $@ $^

medusalog: medusalog.c
	clang -pthread -O2 -g -rdynamic -o $@ $^ -fsanitize=address

clean:
	rm -f medusalog64 medusalog logfile*
	rm -rf log