#!/bin/sh


#JSON_FLOATS
#JSON_64BITS_INTEGERS
#JSON_HEX_NUMBERS

test_basic()
{
	echo "------------------------------------------------------------------------------"
	echo "run test $*"
	cmake $* -DBUILD_TESTS=ON . && make && ./tests
	echo
}

test_hex()
{
	test_basic $* -DJSON_HEX_NUMBERS=ON && test_basic $* -DJSON_HEX_NUMBERS=OFF
}

test_64bit()
{
	test_hex $* -DJSON_64BITS_INTEGERS=ON && test_hex $* -DJSON_64BITS_INTEGERS=OFF
}

test_floats()
{
	test_64bit $* -DJSON_FLOATS=ON && test_64bit $* -DJSON_FLOATS=OFF
}

test_short_next()
{
	test_floats $* -DJSON_SHORT_NEXT=ON && test_64bit $* -DJSON_SHORT_NEXT=OFF
}

test_short_next
