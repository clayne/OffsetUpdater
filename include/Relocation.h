#pragma once

#include <cstdlib>  // size_t
#include <cstdint>  // uint8_t, uintptr_t
#include <string>  // stoi
#include <string_view>  // basic_string_view
#include <vector>  // vector

#include "REL/Relocation.h"


namespace
{
	// https://en.wikipedia.org/wiki/Knuth-Morris-Pratt_algorithm
	constexpr std::size_t NPOS = static_cast<std::size_t>(-1);


	void kmp_table(const std::vector<std::uint8_t>& W, const std::vector<bool>& M, std::vector<std::size_t>& T)
	{
		std::size_t pos = 1;
		std::size_t cnd = 0;

		T[0] = NPOS;

		while (pos < W.size()) {
			if (!M[pos] || !M[cnd] || W[pos] == W[cnd]) {
				T[pos] = T[cnd];
			} else {
				T[pos] = cnd;
				cnd = T[cnd];
				while (cnd != NPOS && M[pos] && M[cnd] && W[pos] != W[cnd]) {
					cnd = T[cnd];
				}
			}
			++pos;
			++cnd;
		}

		T[pos] = cnd;
	}


	std::vector<std::size_t> kmp_search(const std::basic_string_view<std::uint8_t>& S, const std::vector<std::uint8_t>& W, const std::vector<bool>& M)
	{
		std::vector<std::size_t> results;
		std::size_t j = 0;
		std::size_t k = 0;
		std::vector<std::size_t> T(W.size() + 1);
		kmp_table(W, M, T);

		while (j < S.size()) {
			if (!M[k] || W[k] == S[j]) {
				++j;
				++k;
				if (k == W.size()) {
					results.push_back(j - k);
					std::vector<std::uint8_t> matched(W.size());
					for (std::size_t idx = 0; idx < matched.size(); ++idx) {
						matched[idx] = S[idx + results.back()];
					}
					k = T[k];
				}
			} else {
				k = T[k];
				if (k == NPOS) {
					++j;
					++k;
				}
			}
		}

		return results;
	}
}


// pattern scans exe for given sig
class DirectSig
{
public:
	DirectSig() = delete;


	DirectSig(const char* a_sig) :
		_address(0xDEADBEEF),
		_offset(0xDEADBEEF)
	{
		std::vector<std::uint8_t> sig;
		std::vector<bool> mask;
		std::string buf;
		buf.resize(2);
		for (std::size_t i = 0; a_sig[i] != '\0';) {
			switch (a_sig[i]) {
			case ' ':
				++i;
				break;
			case '?':
				mask.push_back(false);
				sig.push_back(0x00);
				do {
					++i;
				} while (a_sig[i] == '?');
				break;
			default:
				mask.push_back(true);
				buf[0] = a_sig[i++];
				buf[1] = a_sig[i++];
				sig.push_back(static_cast<std::uint8_t>(std::stoi(buf, 0, 16)));
				break;
			}
		}

		std::basic_string_view<std::uint8_t> haystack(REL::Module::BasePtr<std::uint8_t>(), REL::Module::Size());
		auto results = kmp_search(haystack, sig, mask);

		if (results.size() != 1) {
			_ERROR("[ERROR] Sig scan failed for pattern (%s)! (%u results found)\n", a_sig, results.size());
		} else {
			_offset = results[0];
			_address = _offset + REL::Module::BaseAddr();
		}
	}


	~DirectSig()
	{}


	std::uintptr_t GetAddress() const
	{
		return _address;
	}


	std::uintptr_t GetOffset() const
	{
		return _offset;
	}

protected:
	mutable std::uintptr_t _address;
	mutable std::uintptr_t _offset;
};


// pattern scans exe for given sig, reads offset from first opcode, and calculates effective address from next op code
class IndirectSig : public DirectSig
{
public:
	IndirectSig() = delete;


	IndirectSig(const char* a_sig) :
		DirectSig(a_sig)
	{
		auto offset = reinterpret_cast<std::int32_t*>(_address + 1);
		auto nextOp = _address + 5;
		_address = nextOp + *offset;
		_offset = _address - REL::Module::BaseAddr();
	}


	~IndirectSig()
	{}
};
