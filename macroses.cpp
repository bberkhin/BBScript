#include "macroses.h"
#include <algorithm>



Macroses &Macroses::getInstance()
{
    static Macroses macroses_q;
    return macroses_q;
}


void Macroses::Load(rapidxml::xml_node<>* macros_node) 
{
    macros_.clear();
    if (!macros_node) return;

    int n = 1;
    for (auto* macro_node = macros_node->first_node("macro"); macro_node; macro_node = macro_node->next_sibling("macro")) {
        Macro m;
        if (auto* label_attr = macro_node->first_attribute("label")) {
            m.name = label_attr->value();
        }
        else {
            m.name = std::string("Macro ") + std::to_string(n++);
        }
        m.code = macro_node->value();
        m.code = trim(m.code);

        macros_.push_back(m);
    }
}

std::string Macroses::trim(const std::string& s) {
    // Удалить \r из всей строки
    std::string cleaned = s;
    cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '\r'), cleaned.end());

    // Обрезать пробельные символы (в начале и конце)
    auto start = std::find_if_not(cleaned.begin(), cleaned.end(), [](char c){
        return std::isspace(static_cast<unsigned char>(c));
    });
    auto end = std::find_if_not(cleaned.rbegin(), cleaned.rend(), [](char c){
        return std::isspace(static_cast<unsigned char>(c));
    }).base();

    if (start >= end) return "";
    return std::string(start, end);
}