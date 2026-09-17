#pragma once
# include <optional>
# include <string>

namespace Crails
{
  namespace Payment
  {
    struct CustomerParams
    {
      std::optional<std::string> email;
      std::optional<std::string> name;
      std::optional<std::string> phone;
    };

    struct Customer : public CustomerParams
    {
      std::string id; // provider's remote identifier
    };
  }
}
