/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_DOM_TREE_HPP
#define INCLUDED_ORCUS_DOM_TREE_HPP

#include "pstring.hpp"
#include "types.hpp"

#include <vector>
#include <ostream>
#include <memory>

namespace orcus {

class xmlns_context;
class dom_tree;

namespace sax {

struct doctype_declaration;

}

namespace dom {

enum class node_t : uint8_t
{
    unset,
    declaration,
    attribute,
    element,
    content
};

struct ORCUS_DLLPUBLIC entity_name
{
    xmlns_id_t ns;
    pstring name;

    entity_name();
    entity_name(xmlns_id_t _ns, const pstring& _name);
};

class ORCUS_DLLPUBLIC const_node
{
    friend class ::orcus::dom_tree;

    struct impl;
    std::unique_ptr<impl> mp_impl;

    const_node(std::unique_ptr<impl>&& _impl);
public:
    const_node();
    const_node(const const_node& other);
    const_node(const_node&& other);

    ~const_node();

    node_t type() const;

    size_t child_count() const;

    const_node child(size_t index) const;

    entity_name name() const;

    pstring value() const;

    void swap(const_node& other);

    const_node& operator= (const const_node& other);
};

} // namespace dom

/**
 * Ordinary DOM tree representing the structure of a XML content in full.
 */
class ORCUS_DLLPUBLIC dom_tree
{
    struct impl;
    std::unique_ptr<impl> mp_impl;

public:
    dom_tree(const dom_tree&) = delete;
    dom_tree& operator= (const dom_tree&) = delete;

    struct attr
    {
        dom::entity_name name;
        pstring value;

        attr(xmlns_id_t _ns, const pstring& _name, const pstring& _value);
    };

    typedef std::vector<attr> attrs_type;

    enum class node_type { element, content };

    struct node
    {
        node_type type;

        node(node_type _type) : type(_type) {}

        virtual ~node() = 0;
        virtual void print(std::ostream& os, const xmlns_context& cxt) const = 0;
    };

    typedef std::vector<std::unique_ptr<node>> nodes_type;

    struct element : public node
    {
        dom::entity_name name;
        attrs_type attrs;
        nodes_type child_nodes;

        element(xmlns_id_t _ns, const pstring& _name);
        virtual void print(std::ostream& os, const xmlns_context& cxt) const;
        virtual ~element();
    };

    struct content : public node
    {
        pstring value;

        content(const pstring& _value);
        virtual void print(std::ostream& os, const xmlns_context& cxt) const;
        virtual ~content();
    };

    dom_tree(xmlns_context& cxt);
    ~dom_tree();

    /**
     * Parse a given XML stream and build the content tree.
     *
     * @param strm XML stream.
     */
    void load(const std::string& strm);

    dom::const_node root() const;

    dom::const_node declaration(const pstring& name) const;

    /**
     * Swap the content with another dom_tree instance.
     *
     * @param other the dom_tree instance to swap the content with.
     */
    void swap(dom_tree& other);

    const sax::doctype_declaration* get_doctype() const;

    void dump_compact(std::ostream& os) const;
};

}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
