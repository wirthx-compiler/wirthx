//
// Created by stefan on 12.07.25.
//

#ifndef TYPEREGISTRY_H
#define TYPEREGISTRY_H

#include <map>
#include <unordered_map>

#include "VariableType.h"

class TypeRegistry
{
    using value_type = std::pair<const std::string, const std::shared_ptr<VariableType>>;

private:
    std::unordered_map<std::string, std::shared_ptr<VariableType>> m_types;

public:
    using Iterator = std::unordered_map<std::string, std::shared_ptr<VariableType>>::iterator;
    using CIterator = std::unordered_map<std::string, std::shared_ptr<VariableType>>::const_iterator;


    TypeRegistry();
    ~TypeRegistry();
    void registerType(const std::string &name, const std::shared_ptr<VariableType> &type);
    std::shared_ptr<VariableType> getType(const std::string &name) const;
    bool hasType(const std::string &name) const { return m_types.contains(name); }


    Iterator begin() noexcept { return m_types.begin(); }
    Iterator end() noexcept { return m_types.end(); }

    CIterator begin() const noexcept { return m_types.cbegin(); }
    CIterator end() const noexcept { return m_types.cend(); }

    CIterator cbegin() const noexcept { return m_types.cbegin(); }
    CIterator cend() const noexcept { return m_types.cend(); }
};


#endif // TYPEREGISTRY_H
