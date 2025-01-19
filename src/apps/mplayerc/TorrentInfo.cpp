/*
 * (C) 2019-2024 see Authors.txt
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * using code from TorrentInfo plug-in for FAR 3.0 https://sourceforge.net/projects/farplugs/
 */

#include "stdafx.h"
#include "TorrentInfo.h"
#include <assert.h>
#include <bcrypt.h>   // for calc SHA-1
#include <winternl.h> // for NT_SUCCESS()

struct bt_node {
	enum data_type {
		dt_string,
		dt_integer,
		dt_list,
		dt_dictionary
	};

	typedef std::vector<bt_node*> bt_list;
	typedef std::map<std::string, bt_node*> bt_dict;

	bt_node(const data_type dt)
		: type(dt)
	{
		switch (dt) {
			case dt_integer:
			case dt_string:
				break;
			case dt_list:
				val.list = DNew bt_list();
				break;
			case dt_dictionary:
				val.dict = DNew bt_dict();
				break;
			default:
				assert(false && L"Unknown bt type");
		}
	}

	~bt_node()
	{
		if (type == dt_list) {
			for (auto& item : *val.list) {
				delete item;
			}
			delete val.list;
		}
		else if (type == dt_dictionary) {
			for (auto& item : *val.dict) {
				delete item.second;
			}
			delete val.dict;
		}
	}

	size_t offset = std::string::npos;
	size_t length = 0;
	data_type type;

	struct {
		std::string string;
		__int64     integer;
		bt_list*    list;
		bt_dict*    dict;
	} val = {};
};

CTorrentInfo::~CTorrentInfo()
{
	delete m_root;
}

const bool CTorrentInfo::Read(const wchar_t* fileName)
{
	assert(m_root == nullptr);
	assert(fileName && *fileName);

	delete m_root;
	m_offset = 0;

	HANDLE hFile = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		return false;
	}
	LARGE_INTEGER file_size = {};
	if (!GetFileSizeEx(hFile, &file_size) || file_size.QuadPart == 0 || file_size.QuadPart > 5LL * MEGABYTE) {
		CloseHandle(hFile);
		return false;
	}
	DWORD bytes_read = static_cast<DWORD>(file_size.QuadPart);
	m_data.resize(static_cast<size_t>(bytes_read));
	if (!ReadFile(hFile, &m_data.front(), bytes_read, &bytes_read, nullptr)) {
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);

	m_root = Decode();
	if (!m_root || m_root->type != bt_node::dt_dictionary || m_root->val.dict->empty()) {
		return false;
	}

	return true;
}

std::wstring CTorrentInfo::Magnet() const
{
	std::wstring magnet;

	const auto hashCode = CalcInfoHash();
	if (!hashCode.empty()) {
		magnet += L"magnet:?xt=urn:btih:";
		magnet += hashCode;

		std::list<std::string> list;

		if (auto announce = Search("announce", m_root); announce && announce->type == bt_node::dt_string) {
			list.emplace_back(announce->val.string);
		}

		if (auto announceList = Search("announce-list", m_root); announceList && announceList->type == bt_node::dt_list) {
			GetAnnounceList(announceList, list);
		}

		list.sort();
		list.unique();

		for (const auto& item : list) {
			magnet += L"&tr=";

			//Encode announce url as param
			for (const auto& ch : item) {
				if (isalnum(ch)) {
					magnet += ch;
				} else {
					wchar_t d[4];
					swprintf_s(d, L"%%%02X", ch);
					magnet += d;
				}
			}
		}
	}

	return magnet;
}

bt_node* CTorrentInfo::Decode()
{
	if (m_offset >= m_data.size()) {
		return nullptr;
	}

	bt_node* ret = nullptr;

	if (m_data[m_offset] >= '0' && m_data[m_offset] <= '9') {
		ret = DNew bt_node(bt_node::dt_string);
		ret->offset = m_offset;
		ret->val.string = ReadString();
		ret->length = m_offset - ret->offset;
	} else if (m_data[m_offset] == 'i') {
		ret = DNew bt_node(bt_node::dt_integer);
		ret->offset = m_offset;
		++m_offset;
		ret->val.integer = ReadInteger();
		ret->length = m_offset - ret->offset;
	} else if (m_data[m_offset] == 'l') {
		ret = DNew bt_node(bt_node::dt_list);
		ret->offset = m_offset;
		++m_offset;
		while (m_offset < m_data.size() && m_data[m_offset] != 'e') {
			auto val = Decode();
			ret->val.list->emplace_back(val);
		}
		++m_offset;
		ret->length = m_offset - ret->offset;
	} else if (m_data[m_offset] == 'd') {
		ret = DNew bt_node(bt_node::dt_dictionary);
		ret->offset = m_offset;
		++m_offset;
		while (m_offset < m_data.size() && m_data[m_offset] != 'e') {
			const auto name = ReadString();
			auto val = Decode();
			ret->val.dict->insert(make_pair(name, val));
		}
		++m_offset;
		ret->length = m_offset - ret->offset;
	}

	return ret;
}

const __int64 CTorrentInfo::ReadInteger()
{
	std::string num;
	while (m_offset < m_data.size() && m_data[m_offset] != 'e') {
		num += m_data[m_offset];
		++m_offset;
	}
	++m_offset; //Skip 'e'

	if (num.find_first_of("dDeE") != std::string::npos) {
		//Exponent found?
		return static_cast<__int64>(atof(num.c_str()));
	}

	return _atoi64(num.c_str());
}

std::string CTorrentInfo::ReadString()
{
	std::string len;
	while (m_offset < m_data.size() && m_data[m_offset] != ':') {
		len += m_data[m_offset];
		++m_offset;
	}
	++m_offset; //Skip ':'

	const auto str_len = atoi(len.c_str());
	if (str_len <= 0 || str_len >= static_cast<int>(m_data.size() - m_offset)) {
		return std::string();
	}

	const std::string ret(&m_data[m_offset], &m_data[m_offset] + str_len);
	m_offset += static_cast<size_t>(str_len);

	return ret;
}

const bt_node* CTorrentInfo::Search(const char* nodeName, const bt_node* nodeParent) const
{
	assert(nodeParent && nodeParent->type == bt_node::dt_dictionary);
	assert(nodeName && *nodeName);

	if (!nodeName || !*nodeName || !nodeParent || nodeParent->type != bt_node::dt_dictionary) {
		return nullptr;
	}

	for (const auto& [name, node] : *nodeParent->val.dict) {
		if (_stricmp(name.c_str(), nodeName) == 0) {
			return node;
		}
	}

	return nullptr;
}

std::wstring CTorrentInfo::CalcInfoHash() const
{
	auto info = Search("info", m_root);
	if (!info || info->type != bt_node::dt_dictionary) {
		return std::wstring();
	}

	//Calculate SHA1 info hash
	std::wstring infoHash;

	BCRYPT_ALG_HANDLE hAlg = nullptr;
	if (NT_SUCCESS(::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, 0))) {
		BCRYPT_HASH_HANDLE hHash = nullptr;
		// As of Windows 7 the hash handle will manage its own object buffer when
		// pbHashObject is nullptr and cbHashObject is 0.
		if (NT_SUCCESS(::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
			// BCryptHashData promises not to modify pbInput.
			if (NT_SUCCESS(::BCryptHashData(hHash, (PUCHAR)&m_data[info->offset], info->length, 0))) {
				UCHAR hash[20] = {};
				if (NT_SUCCESS(::BCryptFinishHash(hHash, hash, std::size(hash), 0))) {
					for (const auto& b : hash) {
						wchar_t d[3];
						swprintf_s(d, L"%02x", b);
						infoHash += d;
					}
				}
			}
			::BCryptDestroyHash(hHash);
		}
		::BCryptCloseAlgorithmProvider(hAlg, 0);
	}

	return infoHash;
}

void CTorrentInfo::GetAnnounceList(const bt_node* nodeAnnounce, std::list<std::string>& list) const
{
	assert(nodeAnnounce && nodeAnnounce->type == bt_node::dt_list);

	if (!nodeAnnounce || nodeAnnounce->type != bt_node::dt_list) {
		return;
	}

	for (auto& item : *nodeAnnounce->val.list) {
		if (item->type == bt_node::dt_string) {
			list.emplace_back(item->val.string);
		} else if (item->type == bt_node::dt_list) {
			GetAnnounceList(item, list);
		}
	}
}
