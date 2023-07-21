order: order.c comm.c helper.c matching_engine.c serializers.c
	clang -o order order.c comm.c helper.c matching_engine.c serializers.c -lhiredis -std=c11

test: test.c helper.c matching_engine.c serializers.c
	clang -o test test.c helper.c matching_engine.c serializers.c -lhiredis -std=c11

test2: test2.c helper.c matching_engine.c serializers.c
	clang -o test2 test2.c helper.c matching_engine.c serializers.c -lhiredis -std=c11

market_data: market_data.c  helper.c
	clang -o market_data market_data.c helper.c -lhiredis -std=c11

exec: exec.c helper.c matching_engine.c serializers.c
	clang -o exec exec.c helper.c matching_engine.c serializers.c -lhiredis -std=c11
