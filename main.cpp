#include <audioeq/core.h>


int main(int argc, char *argv[])
{
	aeq::Core core {argc, argv};

	std::vector<aeq::Node *> nodes;
	core.list_nodes(nodes);

	while (true) {
	}

	return 0;
}
