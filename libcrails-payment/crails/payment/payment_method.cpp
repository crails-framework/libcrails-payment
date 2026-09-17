#include "payment_method.hpp"

using namespace std;

string_view Crails::Payment::to_string(PaymentMethodType type)
{
  switch (type)
  {
  case PaymentMethodType::Card:         return "card";
  case PaymentMethodType::SepaDebit:    return "sepa_debit";
  case PaymentMethodType::Paypal:       return "paypal";
  case PaymentMethodType::BankTransfer: return "bank_transfer";
  case PaymentMethodType::Other:        break ;
  }
  return "other";
}
