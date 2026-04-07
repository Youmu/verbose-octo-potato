#pragma once
#include <string>
#include "psa/crypto.h"

class PotatoRequestBuilder {
 public:
  PotatoRequestBuilder(const std::string &key_base64);
  std::string BuildRequest(const std::string &from, const std::string &message);
private:
  psa_key_id_t key_id;
};
