#!/bin/sh


#JSON_FLOATS
#JSON_64BITS_INTEGERS
#JSON_HEX_NUMBERS

test_basic()
{
	echo "------------------------------------------------------------------------------"
	echo "run test $*"

	local CMAKE_OPTS=""
	local hx fl wi sn ohx ofl owi osn
	hx="-"; fl="-"; wi="-"; sn="-"; pk="-"
	ohx="OFF"; ofl="OFF"; owi="OFF"; osn="OFF"; opk="OFF"
	for a in $*; do
		case $a in
		hx)
			hx="h"
			ohx="ON"
			;;
		fl)
			fl="f"
			ofl="ON"
			;;
		wi)
			wi="i"
			owi="ON"
			;;
		sn)
			sn="s"
			osn="ON"
			;;
		pk)
			pk="p"
			opk="ON"
			;;
		esac
	done

	local name
	name=nj_tests_$hx$wi$fl$sn$pk
	cmake -DJSON_HEX_NUMBERS=$ohx -DJSON_64BITS_INTEGERS=$owi -DJSON_FLOATS=$ofl -DJSON_SHORT_NEXT=$osn -DJSON_PACKED=$opk -DBUILD_TESTS=ON . && make || exit 1
	mv ./tests $name
	echo "./$name || exit 1" >> tests.sh
	echo
}

test_hex()
{
	test_basic $* && test_basic $* hx
}

test_64bit()
{
	test_hex $* && test_hex $* wi
}

test_floats()
{
	test_64bit $* && test_64bit $* fl
}

test_short_next()
{
	test_floats $* && test_floats $* sn
}

test_packet_next()
{
	test_short_next $* && test_short_next $* pk
}

case $1 in
clean)
	rm nj_tests* tests.sh
	exit 0
	;;
esac

echo "#!/bin/sh" > tests.sh
test_packet_next
chmod +x tests.sh
