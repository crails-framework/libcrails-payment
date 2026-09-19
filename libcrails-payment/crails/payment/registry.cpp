#include "registry.hpp"

using namespace std;
using namespace Crails::Payment;

void Registry::add(shared_ptr<const Provider> provider)
{
  providers[provider->name()] = provider;
}

shared_ptr<const Provider> Registry::find(string_view name)
{
  const Registry& self = singleton::require();
  auto            it   = self.providers.find(name);

  return it != self.providers.end() ? it->second : nullptr;
}

const Provider& Registry::get(string_view name)
{
  auto provider = find(name);

  if (!provider)
    throw boost_ext::out_of_range("Crails::Payment::Registry: no provider registered as '" + string(name) + "'");
  return *provider;
}

vector<string_view> Registry::list()
{
  const Registry&     self = singleton::require();
  vector<string_view> result;

  result.reserve(self.providers.size());
  for (auto it = self.providers.begin() ; it != self.providers.end() ; ++it)
    result.push_back(it->first);
  return result;
}
