//
// Created by stefan on 12.07.25.
//

#include "TypeRegistry.h"
TypeRegistry::TypeRegistry() {}
TypeRegistry::~TypeRegistry() {}
void TypeRegistry::registerType(const std::string &name, const std::shared_ptr<VariableType> &type)
{
    m_types[name] = type;
}
std::shared_ptr<VariableType> TypeRegistry::getType(const std::string &name) const { return m_types.at(name); }
