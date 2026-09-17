#include "registry.hpp"

using namespace std;
using namespace Crails::Payment;

void Registry::add(shared_ptr<const Provider> provider)
{
  providers[provider->name()] = provider;
}

shared_ptr<const Provider> Registry::find(const string& name)
{
  Registry& self = singleton::require();
  auto      it   = self.providers.find(name);

  return it != self.providers.end() ? it->second : nullptr;
}

const Provider& Registry::get(const string& name)
{
  auto provider = find(name);

  if (!provider)
    throw boost_ext::out_of_range("Crails::Payment::Registry: no provider registered as '" + name + "'");
  return *provider;
}
