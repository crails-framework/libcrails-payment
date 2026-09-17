#ifndef TWILIDOC
# include <crails/logger.hpp>
# include <spanstream>
# include "../../payment/amount.hpp"

namespace odb
{
  namespace TRAITS_INCLUDE_SQL_BACKEND
  {
    template<>
    class value_traits<Crails::Payment::Amount, CRAILS_ODB_ID_STRING>
    {
    public:
      typedef Crails::Payment::Amount value_type;
      typedef value_type              query_type;
      typedef details::buffer         image_type;

      static void
      set_value(Crails::Payment::Amount& value,
                const details::buffer& b,
                std::size_t n,
                bool is_null)
      {
        std::string_view v(b.data(), n);
        std::ispanstream stream(v);

        try {
          stream >> value;
        } catch (const std::exception& e) {
          Crails::logger << Crails::Logger::Error << "[ODB][Payment::Amount traits] could not unserialize: " << e.what() << Crails::Logger::endl;
        }
      }

      static void
      set_image(details::buffer& b,
                std::size_t& n,
                bool& is_null,
                const Crails::Payment::Amount& value)
      {
        std::ostringstream stream;
        std::string output;

        stream << value;
        output = stream.str();
        n = output.length();
        if (n > b.capacity())
          b.capacity(n);
        std::memcpy(b.data(), output.c_str(), n);
      }
    };
  }
}

#endif
