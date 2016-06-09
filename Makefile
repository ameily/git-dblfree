
all: git-dblfree

git-dblfree: git-dblfree.cpp
	g++ -std=c++11 git-dblfree.cpp -o git-dblfree -l git2

clean:
	rm -f git-dblfree
