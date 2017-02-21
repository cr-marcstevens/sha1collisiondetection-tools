/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/

#ifndef DISTURBANCEVECTOR_HPP
#define DISTURBANCEVECTOR_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <boost/lexical_cast.hpp>
#include <boost/container/flat_map.hpp>

class disturbancevector {
public:
	typedef ::uint32_t uint32;
	uint32 DV[80];
	uint32 DW[80];
	int dvtype;
	int dvk;
	int dvb;

	// specifify DVtype=1,2 K=0-79, b=0-31 
	explicit disturbancevector(int DVtype, int K, int b) 
	{ 
		assign(DVtype, K, b); 
	}

	// specificy 16 consecutive words DV[offset+0],...,DV[offset+15], offset=0,...,64
	explicit disturbancevector(uint32 disturbances[16], int offset) 
	{ 
		assign(disturbances, offset); 
	}

	// specify name: I(K,b) or II(K,b)
	explicit disturbancevector(std::string DVstr) {
		// DVstr = "I(K,b)" or "II(K,b)"
		int DVtype = 0;
		while (DVstr.size() > 0 && DVstr[0] == 'I') {
			++DVtype;
			DVstr.erase(0, 1);
		}
		if (DVtype == 0 || DVtype > 2) throw std::runtime_error("DV string incorrect: " + DVstr);
		// DVstr = "(K,b)"
		if (DVstr.size() == 0 || (DVstr[0] != '(' && DVstr[0] != '_')) throw std::runtime_error("DV string incorrect: " + DVstr);
		DVstr.erase(0,1); 
		// DVstr = "K,b)"
		std::string::size_type pos = DVstr.find_first_of(",_");
		if (pos == std::string::npos) throw std::runtime_error("DV string incorrect: " + DVstr);
		int K = boost::lexical_cast<int>(DVstr.substr(0, pos));
		DVstr.erase(0,pos+1);
		// DVstr = "b)"
		pos = DVstr.find_first_not_of("0123456789");
		int b = boost::lexical_cast<int>(DVstr.substr(0, pos));

		assign(DVtype, K, b);
	}

	std::string name() const {
		for (int K = 0; K <= 64; ++K) {
			bool possible = true;
			for (int i = 4; i <= 14; ++i)
				if (DV[K+i] != 0) {
					possible = false;
					break;
				}
			if (!possible || hammingweight(DV[K+15]) != 1) continue;
			int b = 0;
			while ((DV[K+15]&(1<<b))==0) ++b;
			if (DV[K+1] == 0) { // check if it is I(K,b)
				if (DV[K+0]==0 && DV[K+1]==0 && DV[K+2]==0 && DV[K+3]==0)
					return "I(" + boost::lexical_cast<std::string>(K) + "," + boost::lexical_cast<std::string>(b) + ")";
			} else { // check if it is II(K,b)
				uint32 rb = rotate_left(uint32(1<<31),b);
				if (DV[K+0]==0 && DV[K+1]==rb && DV[K+2]==0 && DV[K+3]==rb)
					return "II(" + boost::lexical_cast<std::string>(K) + "," + boost::lexical_cast<std::string>(b) + ")";
			}
			// other unknown type with parameters K and b
			return "unknown(" + boost::lexical_cast<std::string>(K) + "," + boost::lexical_cast<std::string>(b) + ")";
		}
		return "unknown";
	}

	void assign(int DVtype, int K, int b) 
	{
		dvtype = DVtype; dvk = K; dvb = b;
		if (DVtype < 1 || DVtype > 2 || K < 0 || K > 64 || b < 0 || b >= 32) throw std::runtime_error("bad disturbancevector specification");
		for (unsigned i = unsigned(K); i < unsigned(K + 16); ++i)
			DV[i] = 0;
		DV[K+15] = rotate_left(uint32(1),b);
		if (DVtype == 2) {
			DV[K+1] = rotate_left(uint32(1<<31),b);
			DV[K+3] = rotate_left(uint32(1<<31),b);
		}
		expand_me(DV, K);
		initDW();
	}
	void assign(uint32 disturbances[16], int offset)
	{
		dvtype = dvk = dvb = 0;
		if (offset < 0 || offset > 64) throw std::runtime_error("bad disturbances offset specification");
		for (unsigned i = 0; i < 16; ++i)
			DV[offset+i] = disturbances[i];
		expand_me(DV, offset);
		initDW();
	}
	// determine DW from DV
	void initDW() {
		// determine DW
		for (int i = 16; i < 32; ++i)
			DW[i] = DV[i-0] ^ rotate_left(DV[i-1],5) ^ DV[i-2] ^ rotate_left(DV[i-3],30) ^ rotate_left(DV[i-4],30) ^ rotate_left(DV[i-5],30);
		expand_me(DW, 16);
	}

	// make V an expanded message using the message expansion relation from the 16 sequential words starting at offset in V
	void expand_me(uint32 V[80], int offset) {
		for (int i = offset - 1; i >= 0; --i)
			V[i]=rotate_right(V[i+16], 1) ^ V[i+13] ^ V[i+8] ^ V[i+2];
		for (int i = offset + 16; i < 80; ++i)
			V[i]=rotate_left(V[i-3] ^ V[i-8] ^ V[i-14] ^ V[i-16], 1);
	}


	unsigned hammingweight(uint32 x) const {
		unsigned i = 0;
		while (x) {
			++i;
			x &= x-1;
		}
		return i;
	}
	uint32 rotate_left(uint32 x, unsigned n) const {
		return (x<<n)|(x>>(32-n));
	}
	uint32 rotate_right(uint32 x, unsigned n) const {
		return (x>>n)|(x<<(32-n));
	}
};



#endif // DISTURBANCEVECTOR_HPP
