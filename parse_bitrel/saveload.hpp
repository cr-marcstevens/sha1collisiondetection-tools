/***
* Copyright 2017 Marc Stevens <marc@marc-stevens.nl>, Dan Shumow <danshu@microsoft.com>
* Distributed under the MIT Software License.
* See accompanying file LICENSE.txt or copy at
* https://opensource.org/licenses/MIT
***/

#ifndef HASHCLASH_SAVELOAD_HPP
#define HASHCLASH_SAVELOAD_HPP

#include <vector>
#include <string>
#include <stdexcept>

#ifdef NOSERIALIZATION
#error "Required boost::serialization is disabled. Undefine NOSERIALIZATION."
#endif // NOSERIALIZATION

#pragma warning(push)
#pragma warning(disable: 4267)
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/hash_map.hpp>
#include <boost/serialization/hash_set.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/scoped_ptr.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/slist.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#pragma warning(pop)

namespace hc {

	namespace fs = boost::filesystem;
	using fs::path;

	enum archive_type
	{
		binary_archive = 0, 
		xml_archive, 
		text_archive
	};

	template<class value_type>
	void save(const value_type& val, const std::string& filepath, archive_type artype)
	{
		switch (artype) {
			case binary_archive:
				save(val, artype, path(filepath + ".bin"));
				break;
			case xml_archive:
				save(val, artype, path(filepath + ".xml"));
				break;
			case text_archive:
				save(val, artype, path(filepath + ".txt"));
				break;
			default:
				throw std::runtime_error("save(): archive type not supported!");
		}
	}

	template<class value_type>
	void save(const value_type& val, archive_type artype, const path& filepath)
	{
		switch (artype) {
			case binary_archive:
				{ 
					std::ofstream ofs(filepath.string().c_str(), std::ios::binary);
					if (!ofs) throw std::runtime_error("save(): could not open file!");
					boost::archive::binary_oarchive oa(ofs);
					oa << boost::serialization::make_nvp("saveload", val);
					if (!ofs) throw std::runtime_error("save(): write error!");
				}
				break;

			case xml_archive:
				{
					std::ofstream ofs(filepath.string().c_str());
					if (!ofs) throw std::runtime_error("save(): could not open file!");
					boost::archive::xml_oarchive oa(ofs);
					oa << boost::serialization::make_nvp("saveload", val);
					if (!ofs) throw std::runtime_error("save(): write error!");
				}
				break;

			case text_archive:
				{
					std::ofstream ofs(filepath.string().c_str());
					if (!ofs) throw std::runtime_error("save(): could not open file!");
					boost::archive::text_oarchive oa(ofs);
					oa << boost::serialization::make_nvp("saveload", val);
					if (!ofs) throw std::runtime_error("save(): write error!");
				}
				break;

			default:
				throw std::runtime_error("save(): archive type not supported!");
		}		
	}

	template<class value_type>
	void load(value_type& val, const std::string& filepath, archive_type artype)
	{
		switch (artype) {
			case binary_archive:
				load(val, artype, path(filepath + ".bin"));
				break;
			case xml_archive:
				load(val, artype, path(filepath + ".xml"));
				break;
			case text_archive:
				load(val, artype, path(filepath + ".txt"));
				break;
			default:
				throw std::runtime_error("load(): archive type not supported!");
		}
	}

	template<class value_type>
	void load(value_type& val, archive_type artype, const path& filepath)
	{
		switch (artype)	{
			case binary_archive:
				{
					std::ifstream ifs(filepath.string().c_str(), std::ios::binary);
					if (!ifs) throw std::runtime_error("load(): could not open file!");
					boost::archive::binary_iarchive ia(ifs);
					ia >> boost::serialization::make_nvp("saveload", val);
					if (!ifs) throw std::runtime_error("load(): read error!");	
				}
				break;

			case xml_archive:
				{
					std::ifstream ifs(filepath.string().c_str());
					if (!ifs) throw std::runtime_error("load(): could not open file!");
					boost::archive::xml_iarchive ia(ifs);
					ia >> boost::serialization::make_nvp("saveload", val);
					if (!ifs) throw std::runtime_error("load(): read error!");	
				}
				break;

			case text_archive:
				{
					std::ifstream ifs(filepath.string().c_str());
					if (!ifs) throw std::runtime_error("load(): could not open file!");
					boost::archive::text_iarchive ia(ifs);
					ia >> boost::serialization::make_nvp("saveload", val);
					if (!ifs) throw std::runtime_error("load(): read error!");	
					break;
				}

			default:
				throw std::runtime_error("load(): archive type not supported!");
		}
	}

} // namespace 

#endif // HASHCLASH_SAVELOAD_HPP
