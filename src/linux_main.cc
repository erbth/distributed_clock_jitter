#include <cstdio>
#include <cstdlib>
#include <exception>
#include "linux_system_services.h"
#include "controller.h"

using namespace std;

int main(int argc, char **argv)
{
	try
	{
		if (argc != 2)
		{
			printf ("Usage: %s <interface name>\n", argv[0]);
			return EXIT_FAILURE;
		}

		auto prov = system_services::linux_provider::create(argv[1]);

		auto mac = prov->get_own_mac_address ();
		printf ("Own mac address: %02x:%02x:%02x:%02x:%02x:%02x\n",
				(int) mac[0], (int) mac[1], (int) mac[2],
				(int) mac[3], (int) mac[4], (int) mac[5]);

		controller contr (prov);
		prov->main_loop ();

		return EXIT_SUCCESS;
	}
	catch (exception &e)
	{
		fprintf (stderr, "Error: %s\n", e.what());
		return EXIT_FAILURE;
	}
}
