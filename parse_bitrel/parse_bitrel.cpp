/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/


#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <iomanip>

#include <boost/cstdint.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "disturbancevector.hpp"
#include "saveload.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace std;

typedef boost::uint32_t uint32;

class bitrel;
map<string,bitrel> gl_map_DV_bitrels;


vector<string> break_string(string in, const string& delim)
{
	vector<string> ret;
	size_t pos = in.find_first_of(delim);
	while (pos < in.size()) 
	{
		ret.push_back(in.substr(0, pos));
		in.erase(0, pos+1);
		pos = in.find_first_of(delim);
	} 
	ret.push_back(in);
	return ret;
}


vector<uint32> operator^(const vector<uint32>& l, const vector<uint32>& r)
{
	if (l.size() != r.size()) 
		throw runtime_error("vector xor undefined for unequal length vectors");

	vector<uint32> ret(l);
	for (unsigned i = 0; i < ret.size(); ++i)
		ret[i] ^= r[i];

	return ret;
}


vector<uint32>& operator^=(vector<uint32>& l, const vector<uint32>& r)
{
	if (l.size() != r.size()) 
		throw runtime_error("vector xor undefined for unequal length vectors");

	for (unsigned i = 0; i < l.size(); ++i)
		l[i] ^= r[i];

	return l;
}


class bitrel 
{
public:
	vector< vector<uint32> > basis; // 80 wordmasks + LSB 81-th word as parity 

	size_t size() const 
	{ 
		return basis.size(); 
	}

	void clear() 
	{ 
		basis.clear(); 
	}

	vector< vector<uint32> > space(unsigned len = 81) const 
	{
		vector< vector<uint32> > tmp;
		if (basis.size() == 0) 
			return tmp;

		// skip the zero vector: start with i=1
		for (uint32 i = 1; i < uint32(1<<basis.size()); ++i) 
		{ 
			vector<uint32> elem(basis.front().size(), 0);

			for (uint32 j = 0; j < basis.size(); ++j)
				if (i & (1<<j))
					elem ^= basis[j];

			elem.resize(len);
			tmp.push_back(elem);
		}

		std::sort(tmp.begin(), tmp.end());
		tmp.erase( std::unique(tmp.begin(),tmp.end()), tmp.end());
		return tmp;
	}

	template<typename Archive>
	void serialize(Archive& ar, const unsigned int file_version)
	{
		ar & boost::serialization::make_nvp("basis", basis);
	}
};


string bitrel_to_string(const vector<uint32>& br) 
{
	string ret;

	for (unsigned t = 0; t < 80; ++t)
		if (br[t])
			for (unsigned b = 0; b < 32; ++b)
				if ((br[t]>>b)&1)
					ret += string(ret.empty() ? "W" : " ^ W") + to_string(t) + "[" + to_string(b) + "]";

	if (br.size() > 80)
		ret += " = " + to_string(br[80]&1);
	return ret;
}


vector<uint32> parse_bitrel_line(string in)
{
	//exampleline: - W37[4] ^ W39[4] = 1
	vector<uint32> br(81,0); // 80 wordmasks plus parity

	size_t pos = in.find("=");
	if ((pos = in.find_first_of("01", pos)) == string::npos) 
		throw;
	if (in[pos] == '1') 
		br[80] = 1;

	in.erase(pos);
	while (true) 
	{
		if ((pos = in.find_first_of("0123456789")) == string::npos) 
			break;

		size_t pos2 = in.find_first_not_of("0123456789", pos);
		unsigned t = stoul(in.substr(pos, pos2-pos));

		if ((pos = in.find_first_of("0123456789", pos2)) == string::npos) 
			break;

		pos2 = in.find_first_not_of("0123456789", pos);
		unsigned b = stoul(in.substr(pos, pos2-pos));

		if (t >= 80 || b >= 32) 
			throw runtime_error("t or b out of bounds");
		br[t] ^= uint32(1)<<b;

		in.erase(0, pos2);
	}
	return br;
}


void load_bitrel(bitrel& br, const fs::path& filename)
{
	br.clear();
	ifstream ifs(filename.native());
	while (!!ifs) 
	{
		string line;
		getline(ifs, line);
		if (line.find("=") != string::npos)
			br.basis.push_back( parse_bitrel_line(line) );
	}
}


string filename_to_DV(const fs::path& filename) 
{
	vector<string> DV = break_string( filename.stem().string() , "_-");
	if ( DV.size() >= 3 
		&& (DV[0] == "I" || DV[0] == "II")
		&& DV[1].find_first_not_of("0123456789") == string::npos
		&& DV[2].find_first_not_of("0123456789") == string::npos
		)
	{
		return DV[0] + "(" + DV[1] + "," + DV[2] + ")";
	} 
	throw runtime_error("Filename does not contain DV description");
}


void load_bitrels(std::map<string,bitrel>& map_DV_bitrels, const string& workdir, const set<string>& DVselection)
{
	cout << "Loading bit relation data for DVs from directory " << workdir << endl;
	fs::path basedir = workdir;
	if (!fs::is_directory(basedir)) 
		throw runtime_error("Specified workdir is not a directory");

	for (auto dit = fs::directory_iterator(basedir); dit != fs::directory_iterator(); ++dit)
	{
		if (fs::is_regular_file(dit->path()))
		{
			string DV = filename_to_DV(dit->path());
			if (!DVselection.empty())
			{
				bool ok = false;
				for (auto it = DVselection.begin(); it != DVselection.end(); ++it)
				{
					if ((dit->path().stem().string().find(*it) != string::npos || DV.find(*it) != string::npos)
						&& dit->path().stem().string().find("I" + *it) == string::npos
						&& DV.find("I" + *it) == string::npos
						)
						ok = true;
				}
				if (!ok) continue;
			}

			cout << DV << ": " << flush;
			load_bitrel(map_DV_bitrels[DV], dit->path());
			cout << map_DV_bitrels[DV].size() << endl;
		}
	}
}


unsigned hammingweight(uint32 in)
{
	unsigned c = 0;
	for (; in; ++c)
		in &= in - 1;
	return c;
}

unsigned hammingweight(const vector<uint32>& in)
{
	unsigned c = 0;
	for (auto i = in.begin(); i != in.end(); ++i)
		c += hammingweight(*i);
	return c;
}


bool basis_less(const std::vector<uint32>& l, const std::vector<uint32>& r)
{
	// first: rate on total # active bits
	unsigned hwl = hammingweight(l), hwr = hammingweight(r);
	if (hwl != hwr) 
		return hwl < hwr;

	// second: rate on # active bit positions
	uint32 bitsl = 0, bitsr = 0;
	for (auto it = l.begin(); it != l.end(); ++it) 
		bitsl |= *it;
	for (auto it = r.begin(); it != r.end(); ++it) 
		bitsr |= *it;
	hwl = hammingweight(bitsl); 
	hwr = hammingweight(bitsr);
	if (hwl != hwr) 
		return hwl < hwr;

	// third: rate on maximum worddistance between active bits
	int fl = 0, fr = 0;
	while (fl < (int)l.size() && l[fl] == 0) 
		++fl;
	while (fr < (int)r.size() && r[fr] == 0) 
		++fr;
	int el = l.size()-1, er = r.size()-1;
	while (el > 0 && l[el] == 0) 
		--el;
	while (er > 0 && r[er] == 0) 
		--er;
	if ((el-fl) != (er-fr)) 
		return (el-fl) < (er-fr);

	// fourth: lex
	return l < r;
}


void greedy_selection(const map<string,bitrel>& map_DV_bitrels, map<vector<uint32>, vector<string> >& bitrel_to_DV)
{
	map<string,bitrel> map_DV_newbitrels;
  
	while (true) 
	{
		map<vector<uint32>, vector<string> > bitrelcnt;
		map<vector<uint32>, vector<string> > bitrelcnt2;
		for (auto DVit = map_DV_bitrels.begin(); DVit != map_DV_bitrels.end(); ++DVit) 
		{
			vector< vector<uint32> > fullspace = DVit->second.space(81); // 81 and 80 give the same results => all bitrel do not have negated version for other DV (so far)
			vector< vector<uint32> > selspace = map_DV_newbitrels[DVit->first].space(81);
			for (auto it = fullspace.begin(); it != fullspace.end(); ++it) 
			{
				if (!binary_search(selspace.begin(), selspace.end(), *it))
					bitrelcnt[*it].push_back(DVit->first);
				bitrelcnt2[*it].push_back(DVit->first);
			}
		}

		uint32 maxcnt = 0;
		for (auto it = bitrelcnt.begin(); it != bitrelcnt.end(); ++it)
			if (it->second.size() > maxcnt)
				maxcnt = it->second.size();
		if (maxcnt == 0) break;

		vector< vector<uint32> > maxbitrel;
		for (auto it = bitrelcnt.begin(); it != bitrelcnt.end(); ++it)
			if (it->second.size() == maxcnt)
				maxbitrel.push_back(it->first);
		std::sort(maxbitrel.begin(), maxbitrel.end(), basis_less);
    
		vector<uint32> newbitrel = maxbitrel.front();
		vector<string>& newbitrelDVs = bitrel_to_DV[newbitrel];
		cout << "- " << bitrel_to_string(newbitrel) << ": ";
		for (auto it = bitrelcnt[newbitrel].begin(); it != bitrelcnt[newbitrel].end(); ++it) 
		{
			cout << " " << *it;
			newbitrelDVs.push_back(*it);
			map_DV_newbitrels[*it].basis.push_back(newbitrel);
		}
		cout << " (+" << (bitrelcnt2[newbitrel].size()-bitrelcnt[newbitrel].size()) << "DVs)" << endl;
		std::sort(newbitrelDVs.begin(), newbitrelDVs.end());
	}

	for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it) 
	{
		bool first = true;
		for (auto it2 = bitrel_to_DV.begin(); it2 != bitrel_to_DV.end(); ++it2)
		{
			if (it2 != it && it2->second.size() > 1)
			{
				bool ok = true;
				for (auto it3 = it2->second.begin(); it3 != it2->second.end(); ++it3)
				{
					if (!binary_search(it->second.begin(), it->second.end(), *it3))
					{
						ok = false;
						break;
					}
				}
				if (!ok) 
					continue;

				// it2 is subset of it1
				if (first) 
				{
					first = false;
					cout << bitrel_to_string(it->first) << " (" << it->second.size() << ") => ";
				}
				else 
					cout << " , ";
				cout << bitrel_to_string(it2->first) << " (" << it2->second.size() << ")";
			}
		}
		if (!first) 
			cout << endl;
	}  
}

string DVvariablename(const string& DV, const string& suffix = "", const string& prefix = "DV_") 
{
	string ret = prefix + DV + suffix;
	size_t pos;
	while ( (pos = ret.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")) < ret.size())
		ret[pos] = '_';
	return ret;
}

// returns c expression that should be evaluated as bool (i.e., zero / non-zero integer)
string bitrel_bool_expression(const vector<uint32>& bitrel, const string& Wname = "W")
{
	string ret;
	if ((hammingweight(bitrel) - hammingweight(bitrel[80])) != 2) 
		throw std::runtime_error("bitrel_bool_expression(,): expected bitrelation with only 2 active W bits");
	unsigned t1 = 0,t2 = 79;
	while (bitrel[t1] == 0) 
		++t1;
	while (bitrel[t2] == 0) 
		--t2;
	int b1 = 0, b2 = 31;
	while (0 == ((bitrel[t1]>>b1)&1)) 
		++b1;
	while (0 == ((bitrel[t2]>>b2)&1)) 
		--b2;

	// shift W[t2] bit position b2 to bit position b1, then xor W2 and W1 and keep only bit position b1
	string W1 = Wname + "[" + boost::lexical_cast<string>(t1) + "]";
	string W2 = Wname + "[" + boost::lexical_cast<string>(t2) + "]";

	if (b1 > b2)
		W2 = "(" + W2 + "<<" + boost::lexical_cast<string>(b1-b2) + ")";
	else if (b2 > b1)
		W2 = "(" + W2 + ">>" + boost::lexical_cast<string>(b2-b1) + ")";

	ret = "((" + W1 + "^" + W2 + ") & (1<<" + boost::lexical_cast<string>(b1) + "))";

	if (bitrel[80] != 0)
		return ret;
	else
		return "(!" + ret + ")"; // use ! instead of ~ (e.g. ~W2) since ! can be absorbed into jz or jnz
}


// return c expression that if true returns 0xFFFFFFFF and else 0
string bitrel_c_expression(const vector<uint32>& bitrel, const string& Wname = "W")
{
	if ((hammingweight(bitrel) - hammingweight(bitrel[80])) != 2) 
		throw std::runtime_error("bitrel_c_expression(,): expected bitrelation with only 2 active W bits");
	unsigned t1 = 0,t2 = 79;
	while (bitrel[t1] == 0) 
		++t1;
	while (bitrel[t2] == 0) 
		--t2;
	int b1 = 0, b2 = 31;
	while (0 == ((bitrel[t1]>>b1)&1)) 
		++b1;
	while (0 == ((bitrel[t2]>>b2)&1)) 
		--b2;

	// shift W[t2] bit position b2 to bit position b1, then xor W2 and W1 and keep only bit position b1
	string W1 = Wname + "[" + boost::lexical_cast<string>(t1) + "]";
	string W2 = Wname + "[" + boost::lexical_cast<string>(t2) + "]";

	if (b1 == b2)
		return "(0-(((" + W1 + "^" + (bitrel[80]==0?"~":"") + W2 + ")>>" + boost::lexical_cast<string>(b1) + ")&1))";
	else
		return "(0-(((" + W1 + ">>" + boost::lexical_cast<string>(b1) + ")^(" + (bitrel[80]==0?"~":"") + W2 + ">>" + boost::lexical_cast<string>(b2) + "))&1))";
}


// returns c expression that sets bits in the closed-range [lowbit,highbit] to 1 if true and 0 else
// bits lower than lowbit and bits higher than highbit are undetermined (may be 0 or 1 independent of each other)
string bitrel_c_expression(const vector<uint32>& bitrel, unsigned lowbit, unsigned highbit, const string& Wname = "W")
{
	if ((hammingweight(bitrel) - hammingweight(bitrel[80])) != 2) 
		throw std::runtime_error("bitrel_c_expression(,,,): expected bitrelation with only 2 active W bits");
	unsigned t1 = 0,t2 = 79;
	while (bitrel[t1] == 0) 
		++t1;
	while (bitrel[t2] == 0) 
		--t2;
	unsigned b1 = 0, b2 = 31;
	while (0 == ((bitrel[t1]>>b1)&1)) 
		++b1;
	while (0 == ((bitrel[t2]>>b2)&1)) 
		--b2;

	// shift W[t2] bit position b2 to bit position b1, then xor W2 and W1 and keep only bit position b1
	string W1 = Wname + "[" + boost::lexical_cast<string>(t1) + "]";
	string W2 = Wname + "[" + boost::lexical_cast<string>(t2) + "]";
	// make b1 the lowest bitposition
	if (b1 > b2) {
		std::swap(t1,t2);
		std::swap(b1,b2);
		std::swap(W1,W2);
	}
  
	string ret;

	if (lowbit == highbit) 
	{
		// we can avoid expanding a bit to a full mask, i.e., avoiding an AND and a NEG
		if (b1 == b2) 
		{
			ret = "(" + W1 + "^"  + W2 + ")";
			if (b1 < lowbit) 
				ret = "(" + ret + "<<" + boost::lexical_cast<string>(lowbit - b1) + ")";
			if (b1 > lowbit) 
				ret = "(" + ret + ">>" + boost::lexical_cast<string>(b1 - lowbit) + ")";
			return "(" + string(bitrel[80]==0?"~":"") + ret + ")";
		}
		if (b1 < lowbit)
			W1 = "(" + W1 + "<<" + boost::lexical_cast<string>(lowbit - b1) + ")";
		if (b1 > lowbit) 
			W1 = "(" + W1 + ">>" + boost::lexical_cast<string>(b1 - lowbit) + ")";
		if (b2 < lowbit) 
			W2 = "(" + W2 + "<<" + boost::lexical_cast<string>(lowbit - b2) + ")";
		if (b2 > lowbit) 
			W2 = "(" + W2 + ">>" + boost::lexical_cast<string>(b2 - lowbit) + ")";
		return "(" + string(bitrel[80]==0?"~":"") + "(" + W1 + "^"  + W2 + "))";
	}  

	if (b1 <= lowbit) 
	{
		if (b2 != b1) W2 = "(" + W2 + ">>" + boost::lexical_cast<string>(b2-b1) + ")";
			ret = "((" + W1 + "^" + W2 + ")&(1<<" + boost::lexical_cast<string>(b1) + "))";
		if (bitrel[80]==0)
			return "(" + ret + "-(1<<" + boost::lexical_cast<string>(b1) + "))";
		else
			return "(0-" + ret + ")";
	}

	if (b1 == b2) 
		ret = "(((" + W1 + "^" + W2 + ")>>" + boost::lexical_cast<string>(b1) + ")&1)";
	else 
		ret = "(((" + W1 + ">>" + boost::lexical_cast<string>(b1) + ")^(" + W2 + ">>" + boost::lexical_cast<string>(b2) + "))&1)";
	if (bitrel[80]==0)
		return "(" + ret + "-1)";
	else
		return "(0-" + ret + ")";
}

// returns c expression that sets bits in the closed-range [lowbit,highbit] to 1 if true and 0 else
// bits lower than lowbit and bits higher than highbit are undetermined (may be 0 or 1 independent of each other)
string bitrel_simd_expression(const vector<uint32>& bitrel, unsigned lowbit, unsigned highbit, const string& Wname = "W")
{
	if ((hammingweight(bitrel) - hammingweight(bitrel[80])) != 2)
		throw std::runtime_error("bitrel_c_expression(,,,): expected bitrelation with only 2 active W bits");
	unsigned t1 = 0, t2 = 79;
	while (bitrel[t1] == 0)
		++t1;
	while (bitrel[t2] == 0)
		--t2;
	unsigned b1 = 0, b2 = 31;
	while (0 == ((bitrel[t1] >> b1) & 1))
		++b1;
	while (0 == ((bitrel[t2] >> b2) & 1))
		--b2;

	// shift W[t2] bit position b2 to bit position b1, then xor W2 and W1 and keep only bit position b1
	string W1 = Wname + "[" + boost::lexical_cast<string>(t1)+"]";
	string W2 = Wname + "[" + boost::lexical_cast<string>(t2)+"]";
	// make b1 the lowest bitposition
	if (b1 > b2) {
		std::swap(t1, t2);
		std::swap(b1, b2);
		std::swap(W1, W2);
	}

	string ret;

	if (lowbit == highbit)
	{
		// we can avoid expanding a bit to a full mask, i.e., avoiding an AND and a NEG
		if (b1 == b2)
		{
			ret = "SIMD_XOR_VV(" + W1 + "," + W2 + ")";
			if (b1 < lowbit)
				ret = "SIMD_SHL_V(" + ret + "," + boost::lexical_cast<string>(lowbit - b1) + ")";
			if (b1 > lowbit)
				ret = "SIMD_SHR_V(" + ret + "," + boost::lexical_cast<string>(b1 - lowbit) + ")";
			if (bitrel[80] == 0)
				return "SIMD_NOT_V(" + ret + ")";
			return ret;
		}
		if (b1 < lowbit)
			W1 = "SIMD_SHL_V(" + W1 + "," + boost::lexical_cast<string>(lowbit - b1) + ")";
		if (b1 > lowbit)
			W1 = "SIMD_SHR_V(" + W1 + "," + boost::lexical_cast<string>(b1 - lowbit) + ")";
		if (b2 < lowbit)
			W2 = "SIMD_SHL_V(" + W2 + "," + boost::lexical_cast<string>(lowbit - b2) + ")";
		if (b2 > lowbit)
			W2 = "SIMD_SHR_V(" + W2 + "," + boost::lexical_cast<string>(b2 - lowbit) + ")";
		return string(bitrel[80]==0 ? "SIMD_NOT_V(" : "(") + "SIMD_XOR_VV(" + W1 + "," + W2 + "))";
	}

	if (b1 <= lowbit)
	{
		if (b2 != b1) 
			W2 = "SIMD_SHR_V(" + W2 + "," + boost::lexical_cast<string>(b2 - b1) + ")";
		ret = "SIMD_AND_VW(SIMD_XOR_VV(" + W1 + "," + W2 + "),(1<<" + boost::lexical_cast<string>(b1)+"))";
		if (bitrel[80] == 0)
			return "SIMD_SUB_VW(" + ret + ",(1<<" + boost::lexical_cast<string>(b1)+"))";
		else
			return "SIMD_NEG_V(" + ret + ")";
	}

	if (b1 == b2)
		ret = "SIMD_AND_VW(SIMD_SHR_V(SIMD_XOR_VV(" + W1 + "," + W2 + ")," + boost::lexical_cast<string>(b1)+"),1)";
	else
		ret = "SIMD_AND_VW(SIMD_XOR_VV(SIMD_SHR_V(" + W1 + "," + boost::lexical_cast<string>(b1)+"),SIMD_SHR_V(" + W2 + "," + boost::lexical_cast<string>(b2)+")),1)";
	if (bitrel[80] == 0)
		return "SIMD_SUB_VW(" + ret + ",1)";
	else
		return "SIMD_NEG_V(" + ret + ")";
}


// as a safety precaution, valid testt have zero-diff-state before and after
// depending on implementation, one could add one additional testt either at the beginning or at the end

// in libdetectcoll using step t to test means copying the state between steps t-1 and t of the original run
// and recomputing steps t-1,...,0 backwards and steps t,...,79 forwards
map<string,int> find_testt(const map<string,disturbancevector>& DVs, const map<vector<uint32>, vector<string> >& bitrel_to_DV)
{
	set<string> allDVs;
	map<string,unsigned> DV_nrbitrel;
	for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
	{
		for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
		{
			++DV_nrbitrel[*it2];
			allDVs.insert(*it2);
		}
	}

	map<int,set<string> > t_count;
	for (auto it = DVs.begin(); it != DVs.end(); ++it)
	{
		allDVs.insert(it->first);
		if (it->second.dvtype == 1)
		{
			for (int t = it->second.dvk+5; t <= it->second.dvk+15; ++t)
				t_count[t].insert(it->first);
		}
		else if (it->second.dvtype == 2)
		{
			for (int t = it->second.dvk+9; t <= it->second.dvk+15; ++t)
				t_count[t].insert(it->first);
		}
		else
			throw std::runtime_error("find_testt(): unknown dv type");
	}

	// find smallest set solutions
	map< set<int>, double > solutionst;
	for (unsigned cnt = 1 ; solutionst.size()==0 ; ++cnt)
	{
		vector<bool> sett(t_count.size(), false);
		for (unsigned i = 0; i < cnt; ++i)
			sett[i] = true;
		while (true)
		{
			set<int> ts_covered;
			set<string> DVs_covered;
			for (unsigned i = 0; i < t_count.size(); ++i)
			{
				if (sett[i])
				{
					auto it = t_count.begin(); std::advance(it, i);
					ts_covered.insert(it->first);
					DVs_covered.insert(it->second.begin(), it->second.end());
				}
			}
			if (DVs_covered == allDVs)
				solutionst[ts_covered]=0.0;
			// next permutation in sett
			unsigned j = 0;
			while (j < t_count.size() && sett[j] != true) 
				++j;
			while (j < t_count.size() && sett[j] != false) 
				++j;
			// j points to the first 0 after a 1 has occured
			if (j >= t_count.size()) 
				break; // no next permutation
			// move last 1 prior to j to j-th position
			sett[j] = true;
			sett[j-1] = false;
			// all other previous 1's move to the beginning
			unsigned k = 0;
			for (unsigned l = 0; l < j - 1; ++l)
			{
				if (sett[l])
				{
					std::swap(sett[k], sett[l]);
					++k;
				}
			}
		}
	}
	cout << "Found " << solutionst.size() << " solutions of size " << solutionst.begin()->first.size() << endl;

	// rate solutions
	// TODO
    
	// return best solution
	set<int> solt = solutionst.begin()->first;
	map<string,int> sol;
	for (auto it = solt.begin(); it != solt.end(); ++it)
		for (auto it2 = t_count[*it].begin(); it2 != t_count[*it].end(); ++it2)
			sol[*it2] = *it;
	return sol;
}

void output_code_header(map<string, unsigned>& DV_to_bitpos, const map<vector<uint32>, vector<string> >& bitrel_to_DV, ostream& out_h, ostream& out_c, ostream& out_c_test)
{
	unsigned dvmasksize = ((DV_to_bitpos.size() + 31) / 32);

	map<string, disturbancevector> DVs;
	for (auto it = DV_to_bitpos.begin(); it != DV_to_bitpos.end(); ++it)
		DVs.emplace(it->first, disturbancevector(it->first));
	// figure out distribution of testts: minimum #, balanced distribution of DVs
	map<string, int> DV_testt = find_testt(DVs, bitrel_to_DV);
	set<int> testt;
	for (auto it = DV_testt.begin(); it != DV_testt.end(); ++it)
		testt.insert(it->second);

	out_h << "#ifndef UBC_CHECK_H" << endl;
	out_h << "#define UBC_CHECK_H" << endl << endl;
	out_h << "#include <stdint.h>" << endl << endl;
	out_h << "#define DVMASKSIZE " << dvmasksize << endl;
	out_h << "typedef struct { int dvType; int dvK; int dvB; int testt; int maski; int maskb; uint32_t dm[80]; } dv_info_t;" << endl;
	out_h << "extern dv_info_t sha1_dvs[];" << endl;
	out_h << "void ubc_check(const uint32_t W[80], uint32_t dvmask[DVMASKSIZE]);" << endl;

	out_h << endl;
	for (auto it = testt.begin(); it != testt.end(); ++it)
		out_h << "#define DOSTORESTATE" << std::setw(2) << std::setfill('0') << *it << endl;
	out_h << endl;

	out_h << endl << "#endif // UBC_CHECK_H" << endl;

	string inttype = (DV_to_bitpos.size() <= 32) ? "uint32_t" : "uint64_t";
	out_c
		<< "#include <stdint.h>" << endl
		<< "#include \"ubc_check.h\"" << endl
		<< endl;
	for (auto it = DV_to_bitpos.begin(); it != DV_to_bitpos.end(); ++it) 
		out_c << "static const " << inttype << " " << DVvariablename(it->first, "bit") << " \t= (" << inttype << ")(1) << " << it->second << ";" << endl;
	out_c << endl;
    
  
	out_c << "dv_info_t sha1_dvs[] = \n{" << endl;
	for (auto it = DVs.begin(); it != DVs.end(); ++it)
	{
		out_c << ((it == DVs.begin())?"  ":", ");
		out_c << "{" << it->second.dvtype << "," << it->second.dvk << "," << it->second.dvb << "," << DV_testt[it->first] << "," << (DV_to_bitpos[it->first]/32) << "," << (DV_to_bitpos[it->first]%32) << ", { ";
		for (int t = 0; t < 80; ++t)
			out_c << ((t!=0)?",":"") << "0x" << std::hex << std::setfill('0') << std::setw(8) << it->second.DW[t] << std::dec;
		out_c << " } }" << endl;
	}
	out_c << ", {0,0,0,0,0,0, {0";
	for (int i = 1; i < 80; ++i)
		out_c << ",0";
	out_c << "}}\n};" << endl;
  



	out_c_test
		<< "#include <stdint.h>" << endl
		<< "#include \"ubc_check.h\"" << endl
		<< endl;

	out_c_test 
		<< "void ubc_check_verify(const uint32_t W[80], uint32_t dvmask[DVMASKSIZE])\n{" << endl
		<< "\tfor (unsigned i=0; i < DVMASKSIZE; ++i)\n\t\tdvmask[i]=0xFFFFFFFF;\n\n";
	for (auto DVit = gl_map_DV_bitrels.begin(); DVit != gl_map_DV_bitrels.end(); ++DVit)
	{
		out_c_test << "\tif (\t   ";
		for (auto it = DVit->second.basis.begin(); it != DVit->second.basis.end(); ++it)
		{
			if (it != DVit->second.basis.begin()) 
				out_c_test << "\t\t|| ";
			out_c_test << "(0";
			for (unsigned i = 0; i < 80; ++i)
			{
				for (unsigned b = 0; b < 32; ++b)
				{
					if ((*it)[i] & (1 << b))
					{
						out_c_test << "^((W[" << i << "]>>" << b << ")&1)";
					}
				}
			}
			out_c_test << ")!=" << (((*it)[80]) ? "1" : "0") << endl;
		}  
		out_c_test << "\t\t)\n\t\t\tdvmask[" << (DV_to_bitpos[DVit->first] / 32) << "] &= ~((uint32_t)(1<<" << (DV_to_bitpos[DVit->first] % 32) << "));\n" << endl;
	}
	out_c_test << "}" << endl;
}



void output_code_simd(const map<vector<uint32>, vector<string> >& bitrel_to_DV, ostream& out_c)
{
	cout << "Generating code..." << endl;

	map<string, unsigned> DV_to_bitpos;
	for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
		for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
			DV_to_bitpos[*it2];
	unsigned DVcnt = 0;
	for (auto it = DV_to_bitpos.begin(); it != DV_to_bitpos.end(); ++it, ++DVcnt)
		it->second = DVcnt;
	if (DVcnt > 64)
	{
		cerr << "Error: Integer type with more than 64 bits required..." << endl;
		return;
	}
	string inttype = (DV_to_bitpos.size() <= 32) ? "uint32_t" : "uint64_t";

	out_c << "#include \"ubc_check.h\"" << endl;
	out_c << endl;
	for (auto it = DV_to_bitpos.begin(); it != DV_to_bitpos.end(); ++it)
		out_c << "static const " << inttype << " " << DVvariablename(it->first, "bit") << " \t= (" << inttype << ")(1) << " << it->second << ";" << endl;
	out_c << endl;
	out_c << "void UBC_CHECK_SIMD(const SIMD_WORD* W, SIMD_WORD* dvmask)" << endl;
	out_c << "{" << endl;
	out_c << "\tSIMD_WORD mask = SIMD_WTOV(0xFFFFFFFF);" << endl;

	for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
	{
		unsigned lowbit = 31, highbit = 0;
		string DVsmask = "(";
		for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
		{
			DVsmask += (it2 == it->second.begin() ? "" : "|") + DVvariablename(*it2, "bit");
			if (DV_to_bitpos[*it2] < lowbit)
				lowbit = DV_to_bitpos[*it2];
			if (DV_to_bitpos[*it2] > highbit)
				highbit = DV_to_bitpos[*it2];
		}
		DVsmask += ")";
		out_c << "\tmask = SIMD_AND_VV(mask, SIMD_OR_VW(" << bitrel_simd_expression(it->first, lowbit, highbit) << ", ~" << DVsmask << "));" << endl;
	}

	out_c << "\tdvmask[0]=mask;" << endl;
	out_c << "}" << endl;;
}




void output_code_v1(const map<vector<uint32>, vector<string> >& bitrel_to_DV, ostream& out_h, ostream& out_c, ostream& out_c_test, unsigned minDVs = 1)
{
	cout << "Generating code..." << endl;
  
	map<string, unsigned> DV_to_bitpos;
	for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
		for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
			DV_to_bitpos[*it2];
	unsigned DVcnt = 0;
	for (auto it = DV_to_bitpos.begin(); it != DV_to_bitpos.end(); ++it,++DVcnt)
		it->second = DVcnt;
	if (DVcnt > 64) 
	{
		cerr << "Error: Integer type with more than 64 bits required..." << endl;
		return;
	}
	string inttype = (DV_to_bitpos.size() <= 32) ? "uint32_t" : "uint64_t";
  
	output_code_header(DV_to_bitpos, bitrel_to_DV, out_h, out_c, out_c_test);
  
	out_c << "void ubc_check(const uint32_t W[80], uint32_t dvmask[" << ((DV_to_bitpos.size()+31)/32)<< "])\n{\n\t" << inttype << " mask = ~((" << inttype << ")(0));\n";
  
	// first process all multi-DV bitrels
	out_c << "\tmask = mask\n";
	//  for (unsigned nrdvs = DVcnt; nrdvs >= minDVs; --nrdvs)
	for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
		if (it->second.size() >= minDVs)
		{
			unsigned lowbit = 31, highbit = 0;
			string DVsmask = "(";
			for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) 
			{
				DVsmask += (it2==it->second.begin()?"":"|") + DVvariablename(*it2, "bit");
				if (DV_to_bitpos[*it2] < lowbit) 
					lowbit = DV_to_bitpos[*it2];
				if (DV_to_bitpos[*it2] > highbit) 
					highbit = DV_to_bitpos[*it2];
			}
			DVsmask += ")";
			out_c << "\t\t & ( " << bitrel_c_expression(it->first,lowbit,highbit) << " | ~" << DVsmask << ")" << endl;
		}
	out_c << "\t\t;\n\n";

	if (minDVs > 1) 
		out_c << "if (mask) {\n" << endl;  
	// now conditionally process remaining DV-specific bitrels
	for (auto DVit = DV_to_bitpos.begin(); DVit != DV_to_bitpos.end(); ++DVit) 
	{
		unsigned bitrelcnt = 0;
		for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
			if (it->second.size() < minDVs && std::find(it->second.begin(),it->second.end(),DVit->first)!=it->second.end())
				++bitrelcnt;
		if (bitrelcnt == 0) 
			continue;
    
		out_c << "\tif (mask & " << DVvariablename(DVit->first, "bit") << ")\n";
		out_c << "\t\t if (\n";
		bool first = true;
		for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
			if (it->second.size() < minDVs && std::find(it->second.begin(),it->second.end(),DVit->first)!=it->second.end())
			{
				if (first) 
				{
					out_c << "\t\t\t    ";
					first = false;
				} else
					out_c << "\t\t\t || ";
				out_c << "!" << bitrel_bool_expression(it->first) << "\n";
			}
		out_c << "\t\t )  mask &= ~" << DVvariablename(DVit->first, "bit") << ";\n";
	}
	if (minDVs > 1) 
		out_c << "}\n" << endl;  
	if (DVcnt <= 32)
		out_c << "\tdvmask[0]=mask;" << endl;
	else
		out_c << "\tdvmask[0]=(uint32_t)(mask);\n\tdvmask[1]=(uint32_t)(mask>>32);" << endl;
	out_c << "}" << endl;; 
}


void output_code_v2(const map<vector<uint32>, vector<string> >& bitrel_to_DV, ostream& out_h, ostream& out_c, ostream& out_c_test, double minprob = 0.5)
{
	cout << "Generating code..." << endl;
  
	map<string, unsigned> DV_to_bitpos;
	for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
		for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
			DV_to_bitpos[*it2];
	unsigned DVcnt = 0;
	for (auto it = DV_to_bitpos.begin(); it != DV_to_bitpos.end(); ++it,++DVcnt)
		it->second = DVcnt;
	if (DVcnt > 64) 
	{
		cerr << "Error: Integer type with more than 64 bits required..." << endl;
		return;
	}
	string inttype = (DV_to_bitpos.size() <= 32) ? "uint32_t" : "uint64_t";
  
	output_code_header(DV_to_bitpos, bitrel_to_DV, out_h, out_c, out_c_test);
  
	out_c << "void ubc_check(const uint32_t W[80], uint32_t dvmask[" << ((DV_to_bitpos.size()+31)/32)<< "])\n{\n\t" << inttype << " mask = ~((" << inttype << ")(0));\n";
  
	// first process all multi-DV bitrels in order from most #DVs to only 2 DVs
	map<string, unsigned> DV_proc_bitrel_cnt;
	for (unsigned nrdvs = DVcnt; nrdvs > 1; --nrdvs) 
	{
		for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
			if (it->second.size() == nrdvs)
			{
				double prob_ub_est = 0.0;
				unsigned lowbit = 31, highbit = 0;
				string DVsmask = "(";
				for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) 
				{
					DVsmask += (it2==it->second.begin()?"":"|") + DVvariablename(*it2, "bit");
					if (DV_to_bitpos[*it2] < lowbit) 
						lowbit = DV_to_bitpos[*it2];
					if (DV_to_bitpos[*it2] > highbit) 
						highbit = DV_to_bitpos[*it2];
					prob_ub_est += double(1)/double(1 << DV_proc_bitrel_cnt[*it2]);
					++DV_proc_bitrel_cnt[*it2];
				}
				DVsmask += ")";

#if 1
				if (prob_ub_est <= minprob)
					out_c << "\tif (mask & " + DVsmask + ")\n\t";
				out_c << "\tmask &= (" << bitrel_c_expression(it->first,lowbit,highbit) << " | ~" << DVsmask << ");" << endl;
#else      
				if (prob_ub_est <= minprob)
					out_c << "\tif ((mask & " + DVsmask + ") && !" << bitrel_bool_expression(it->first) << ")" << endl;
				else
					out_c << "\tif (!" << bitrel_bool_expression(it->first) << ")" << endl;
				out_c << "\t\tmask &=  ~" + DVsmask + ";" << endl;
#endif
			}
	}

	out_c << "if (mask) {\n" << endl;
	// now conditionally process remaining DV-specific bitrels
	for (auto DVit = DV_to_bitpos.begin(); DVit != DV_to_bitpos.end(); ++DVit) 
	{
		unsigned bitrelcnt = 0;
		for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
			if (it->second.size() == 1 && it->second.front() == DVit->first)
				++bitrelcnt;
		if (bitrelcnt == 0) 
			continue;
		if (bitrelcnt == 1)
		{
			out_c << "\tif (mask & " << DVvariablename(DVit->first, "bit") << ")\n";
			unsigned bit=DVit->second;
			for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
				if (it->second.size() == 1 && it->second.front() == DVit->first)
					out_c << "\t\tmask &= (" << bitrel_c_expression(it->first,bit,bit) << " | ~" << DVvariablename(DVit->first, "bit") << ");" << endl;
			continue;
		}
    
		out_c << "\tif (mask & " << DVvariablename(DVit->first, "bit") << ")\n";
		out_c << "\t\t if (\n";
		bool first = true;
		for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
		{
			if (it->second.size() == 1 && it->second.front() == DVit->first)
			{
				if (first)
				{
					out_c << "\t\t\t    ";
					first = false;
				}
				else
					out_c << "\t\t\t || ";
				out_c << "!" << bitrel_bool_expression(it->first) << "\n";
			}
		}
		out_c << "\t\t )  mask &= ~" << DVvariablename(DVit->first, "bit") << ";\n";
	}
	out_c << "}\n" << endl;
	if (DVcnt <= 32)
		out_c << "\tdvmask[0]=mask;" << endl;
	else
		out_c << "\tdvmask[0]=(uint32_t)(mask);\n\tdvmask[1]=(uint32_t)(mask>>32);" << endl;
	out_c << "}" << endl;; 
}


void output_code_v3(const map<vector<uint32>, vector<string> >& bitrel_to_DV, ostream& out_h, ostream& out_c, ostream& out_c_test)
{
	cout << "Generating code..." << endl;
  
	map<string, unsigned> DV_to_bitpos;
	for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
		for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
			DV_to_bitpos[*it2];
	unsigned DVcnt = 0;
	for (auto it = DV_to_bitpos.begin(); it != DV_to_bitpos.end(); ++it,++DVcnt)
		it->second = DVcnt;
	if (DVcnt > 64) 
	{
		cerr << "Error: Integer type with more than 64 bits required..." << endl;
		return;
	}
	string inttype = (DV_to_bitpos.size() <= 32) ? "uint32_t" : "uint64_t";
  
	output_code_header(DV_to_bitpos, bitrel_to_DV, out_h, out_c, out_c_test);
  
	out_c << "void ubc_check(const uint32_t W[80], uint32_t dvmask[" << ((DV_to_bitpos.size()+31)/32)<< "])\n{\n\t" << inttype << " mask = ~((" << inttype << ")(0));\n";
  
	// now conditionally process remaining DV-specific bitrels
	for (auto DVit = DV_to_bitpos.begin(); DVit != DV_to_bitpos.end(); ++DVit) 
	{
		unsigned bitrelcnt = 0;
		for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
			if (std::find(it->second.begin(), it->second.end(), DVit->first) != it->second.end())
				++bitrelcnt;
		if (bitrelcnt == 0) 
			continue;
    
		out_c << "\t if (\t    ";
		bool first = true;
		for (auto it = bitrel_to_DV.begin(); it != bitrel_to_DV.end(); ++it)
		{
			if (std::find(it->second.begin(), it->second.end(), DVit->first) != it->second.end())
			{
				if (first)
					first = false;
				else
					out_c << "\t\t || ";
				out_c << "!" << bitrel_bool_expression(it->first) << "\n";
			}
		}
		out_c << "\t )  mask &= ~" << DVvariablename(DVit->first, "bit") << ";\n";
	}
	if (DVcnt <= 32)
		out_c << "\tdvmask[0]=mask;" << endl;
	else
		out_c << "\tdvmask[0]=(uint32_t)(mask);\n\tdvmask[1]=(uint32_t)(mask>>32);" << endl;
	out_c << "}" << endl;; 
}




int main(int argc, char** argv)
{
	try 
	{

		string ubcdir, outdir;
		vector<string> DVs;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Show options")
			("ubcdir,w", po::value<string>(&ubcdir)->default_value("../data/3565"), "Set directory containing ubc's for each DV")
			("outdir,o", po::value<string>(&outdir)->default_value("../../lib"), "Set directory to output ubc_check{.c,.h,_test.c}")
			("DV,d", po::value< vector<string> >(&DVs), "Select DVs (if not specified uses all DVs in workdir)")
			("store,s", "Store intermediate results")
			("load,l", "Load intermediate results")
			;
		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);
  
		if (vm.count("help") || vm.count("ubcdir")==0) 
		{
			cout << desc << endl;
			return 0;
		}
  
		set<string> DVselection(DVs.begin(), DVs.end());
		map<vector<uint32>, vector<string> > bitrel_to_DV;

		if (vm.count("load")) 
		{
			vector<string> _DVs;
			set<string> _DVselection;
			map<string, bitrel> _gl_map_DV_bitrels;
			map<vector<uint32>, vector<string> > _bitrel_to_DV;
			try
			{
				cout << "Loading previously stored intermediate results." << flush;
				hc::load(_DVs, "data_DVs", hc::binary_archive); cout << "." << flush;
				hc::load(_gl_map_DV_bitrels, "data_map_DV_bitrels", hc::binary_archive); cout << "." << flush;
				hc::load(_DVselection, "data_DVselection", hc::binary_archive); cout << "." << flush;
				hc::load(_bitrel_to_DV, "data_bitrel_to_DV", hc::binary_archive); cout << "." << flush;
				cout << " done." << endl;
			}
			catch (std::exception&)
			{
				_bitrel_to_DV.clear();
				cout << " failed!" << endl;
			}
			if (!_bitrel_to_DV.empty())
			{
				DVs = std::move(_DVs);
				DVselection = std::move(_DVselection);
				gl_map_DV_bitrels = std::move(_gl_map_DV_bitrels);
				bitrel_to_DV = std::move(_bitrel_to_DV);
			}
		} 

		if (bitrel_to_DV.empty())
		{
			load_bitrels(gl_map_DV_bitrels, ubcdir, DVselection);
  
			cout << "Applying greedy selection to exploit overlap of unavoidable bit relation space between DVs..." << endl;
			greedy_selection(gl_map_DV_bitrels, bitrel_to_DV);

			if (vm.count("store")) 
			{
				cout << "Storing intermediate results" << flush;
				hc::save(DVs, "data_DVs", hc::binary_archive); cout << "." << flush;
				hc::save(gl_map_DV_bitrels, "data_map_DV_bitrels", hc::binary_archive); cout << "." << flush;
				hc::save(DVselection, "data_DVselection", hc::binary_archive); cout << "." << flush;
				hc::save(bitrel_to_DV, "data_bitrel_to_DV", hc::binary_archive); cout << "." << flush;
				cout << endl;
			}
		}


		// timings:
		// v2 (0.05) : 10.12s  // fastest
		// v1 (2)    : 12.41s  
		// v1 (1)    : 16.18s  // constant-time
		// v3        : 25.55s
		double totc = 0;
		for (auto it = gl_map_DV_bitrels.begin(); it != gl_map_DV_bitrels.end(); ++it)
		{
			cout << it->first << ": " << it->second.basis.size() << endl;
			totc += double(1)/double(1<<it->second.basis.size());
		}
		cout << totc << " = 2^ " << log(totc)/log(2.0) << endl;
  

		cout << "Generating code files in directory " << outdir << endl;
		string c_name = outdir+"/ubc_check.c";
		string h_name = outdir+"/ubc_check.h";
		string c_test_name = outdir+"/ubc_check_verify.c";
		string c_simd_name = outdir + "/ubc_check_simd.cinc";

		ofstream ofs_c(c_name.c_str(), ios::out | ios::trunc);
		if (!ofs_c)
			throw std::runtime_error("Could not open " + c_name);
		ofstream ofs_h(h_name.c_str(), ios::out | ios::trunc);
		if (!ofs_h)
			throw std::runtime_error("Could not open " + h_name);
		ofstream ofs_c_test(c_test_name.c_str(), ios::out | ios::trunc);
		if (!ofs_c_test)
			throw std::runtime_error("Could not open " + c_test_name);
		ofstream ofs_c_simd(c_simd_name.c_str(), ios::out | ios::trunc);
		if (!ofs_c_simd)
			throw std::runtime_error("Could not open " + c_simd_name);

		output_code_simd(bitrel_to_DV, ofs_c_simd);

#if 0
		//  v3
		// very stupid straightward way: just ifs per DV
		// doesn't use redundency between DV's
		output_code_v3(bitrel_to_DV, ofs_h, ofs_c, ofs_c_test);
#endif 

#if 0
		//  v1: second-fastest, constant-time if minDVs=1
		// first produces a constant-time section using bitrel with #DVs >= minDVs-parameter
		//   bitrels are ordered based on the bits involved to allow further local optimizations by the compiler
		// secondly produces a straightforward if-section like v3, per DV checks its remaining bitrels without further using redundency
		// minDVs=2 clearly optimal
		unsigned minDVs = 1;
		output_code_v1(bitrel_to_DV, ofs_h, ofs_c, ofs_c_test, minDVs);
#endif

#if 1
		//  v2: fastest
		// first produces a constant-time section using bitrel with #DVs from high to low
		//   bitrels are only included if the estimated probability 
		//   that one of its DV's is still active (after checking the previous bitrels) 
		//   is at least the minprob parameter
		// secondly produces a straightforward if-section like v3, per DV checks its remaining bitrels without further using redundency
		// optimum lies between 0.16 and 0.08: 0.1
		double minprob = 0.1;
		output_code_v2(bitrel_to_DV, ofs_h, ofs_c, ofs_c_test, minprob);
#endif

		ofs_c.close();
		ofs_h.close();
		ofs_c_test.close();

	} 
	catch (exception & e) 
	{
		cerr << "Exception: " << e.what() << endl; 
	} 
	catch (...) 
	{
		cerr << "Unknown exception" << endl;
	}
	return 0;
}
