order: order.c comm.c helper.c matching_engine.c serializers.c
	gcc -o order order.c comm.c helper.c matching_engine.c serializers.c -lhiredis --std=c11 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809

test: test.c helper.c matching_engine.c serializers.c
	gcc -o test test.c helper.c matching_engine.c serializers.c -lhiredis --std=c11 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809

test2: test2.c helper.c matching_engine.c serializers.c
	gcc -o test2 test2.c helper.c matching_engine.c serializers.c -lhiredis --std=c11 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809

market_data: market_data.c  helper.c
	gcc -o market_data market_data.c helper.c -lhiredis --std=c11 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809

exec: exec.c helper.c matching_engine.c serializers.c
	gcc -o exec exec.c helper.c matching_engine.c serializers.c -lhiredis --std=c11 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809
