/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/orcus_xml.hpp"
#include "orcus/global.hpp"
#include "orcus/sax_ns_parser.hpp"
#include "orcus/xml_namespace.hpp"
#include "orcus/stream.hpp"
#include "orcus/dom_tree.hpp"

#include "orcus/spreadsheet/factory.hpp"
#include "orcus/spreadsheet/document.hpp"

#include <cstdlib>
#include <cassert>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

#include <boost/filesystem.hpp>

using namespace std;
using namespace orcus;
namespace fs = boost::filesystem;

namespace {

struct test_case
{
    const char* base_dir;
    bool output_equals_input;
};

const std::vector<test_case> tests =
{
    { SRCDIR"/test/xml-mapped/attribute-basic", true },
    { SRCDIR"/test/xml-mapped/attribute-namespace", true },
    { SRCDIR"/test/xml-mapped/attribute-range-self-close", true },
    { SRCDIR"/test/xml-mapped/attribute-single-element", true },
    { SRCDIR"/test/xml-mapped/attribute-single-element-2", true },
    { SRCDIR"/test/xml-mapped/content-basic", true },
    { SRCDIR"/test/xml-mapped/content-namespace", false },
    { SRCDIR"/test/xml-mapped/content-namespace-2", true },
    { SRCDIR"/test/xml-mapped/fuel-economy", true },
    { SRCDIR"/test/xml-mapped/nested-repeats", false },
    { SRCDIR"/test/xml-mapped/nested-repeats-2", false },
    { SRCDIR"/test/xml-mapped/nested-repeats-3", false },
    { SRCDIR"/test/xml-mapped/nested-repeats-4", false },
};

const char* temp_output_xml = "out.xml";

void dump_xml_structure(string& dump_content, string& strm, const char* filepath, xmlns_context& cxt)
{
    file_content content(filepath);
    dom::document_tree tree(cxt);
    tree.load(content.data(), content.size());
    ostringstream os;
    tree.dump_compact(os);
    dump_content = os.str();
}

void test_mapped_xml_import()
{
    string strm;

    for (const test_case& tc : tests)
    {
        fs::path base_dir(tc.base_dir);
        fs::path data_file = base_dir / "input.xml";
        fs::path map_file = base_dir / "map.xml";
        fs::path check_file = base_dir / "check.txt";

        // Load the data file content.
        cout << "reading " << data_file.string() << endl;
        file_content content(data_file.string().data());
        string data_strm = content.str().str();

        spreadsheet::range_size_t ss{1048576, 16384};
        spreadsheet::document doc{ss};
        spreadsheet::import_factory import_fact(doc);
        spreadsheet::export_factory export_fact(doc);

        xmlns_repository repo;
        xmlns_context cxt = repo.create_context();

        // Parse the map file to define map rules, and parse the data file.
        orcus_xml app(repo, &import_fact, &export_fact);
        file_content map_content(map_file.string().data());
        app.read_map_definition(map_content.data(), map_content.size());
        app.read_stream(data_strm.data(), data_strm.size());

        // Zero the source data stream to make sure it's completely erased off
        // memory.
        std::for_each(data_strm.begin(), data_strm.end(), [](char& c) { c = '\0'; });
        assert(data_strm[0] == '\0');
        assert(data_strm[data_strm.size()-1] == '\0');

        // Check the content of the document against static check file.
        ostringstream os;
        doc.dump_check(os);
        string loaded = os.str();
        content.load(check_file.string().data());
        strm = content.str().str();

        assert(!loaded.empty());
        assert(!strm.empty());

        pstring p1(loaded.data(), loaded.size()), p2(strm.data(), strm.size());

        p1 = p1.trim();
        p2 = p2.trim();
        assert(p1 == p2);

        if (tc.output_equals_input)
        {
            // Output to xml file with the linked values coming from the document.
            string out_file = temp_output_xml;
            cout << "writing to " << out_file << endl;
            {
                // Create a duplicate source XML stream.
                content.load(data_file.string().data());
                string data_strm_dup = content.str().str();
                std::ofstream file(out_file);
                assert(file);
                app.write(data_strm_dup.data(), data_strm_dup.size(), file);
            }

            // Compare the logical xml content of the output xml with the
            // input one. They should be identical.

            string dump_input, dump_output;
            string strm_data_file, strm_out_file; // Hold the stream content in memory while the namespace context is being used.
            dump_xml_structure(dump_input, strm_data_file, data_file.string().data(), cxt);
            dump_xml_structure(dump_output, strm_out_file, out_file.data(), cxt);
            assert(!dump_input.empty() && !dump_output.empty());

            cout << dump_input << endl;
            cout << "--" << endl;
            cout << dump_output << endl;
            assert(dump_input == dump_output);

            // Delete the temporary xml output.
            fs::remove(out_file.c_str());
        }
    }
}

void test_invalid_map_definition()
{
    xmlns_repository repo;

    spreadsheet::range_size_t ss{1048576, 16384};
    spreadsheet::document doc{ss};
    spreadsheet::import_factory import_fact(doc);
    orcus_xml app(repo, &import_fact, nullptr);

    try
    {
        app.read_map_definition(ORCUS_ASCII("asdfdasf"));
        assert(false); // We were expecting an exception, but didn't get one.
    }
    catch (const invalid_map_error&)
    {
        // Success!
    }
    catch (const std::exception& e)
    {
        cerr << e.what() << endl;
        assert(!"Wrong exception thrown.");
    }
}

} // anonymous namespace

int main()
{
    test_mapped_xml_import();
    test_invalid_map_definition();

    return EXIT_SUCCESS;
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
