
#include <orcus/spreadsheet/document.hpp>
#include <orcus/spreadsheet/factory.hpp>
#include <orcus/orcus_ods.hpp>

#include <ixion/model_context.hpp>
#include <iostream>

using namespace orcus;

int main()
{
    // Instantiate a document, and wrap it with a factory.
    spreadsheet::document doc;
    spreadsheet::import_factory factory(doc);

    // Pass the factory to the document loader, and read the content from a file
    // to populate the document.
    orcus_ods loader(&factory);
    loader.read_file(SRCDIR"/doc_example/files/document.ods");

    // Now that the document is fully populated, access its content.
    const ixion::model_context& model = doc.get_model_context();

    for (spreadsheet::row_t row = 1; row <= 6; ++row)
    {
        ixion::abs_address_t pos(0, row, 0);
        double value = model.get_numeric_value(pos);
        std::cout << "A" << (pos.row+1) << ": " << value << std::endl;
    }

    return EXIT_SUCCESS;
}