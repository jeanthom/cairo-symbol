#include <string>
#include <iostream>
#include <cairommconfig.h>
#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <cmath>

void drawRTLText(Cairo::RefPtr<Cairo::Context> ctx, const std::string& str) {
    Cairo::TextExtents extents;
    ctx->get_text_extents(str, extents);
    ctx->rel_move_to(-extents.width, 0);
    ctx->show_text(str);
}

enum PinDirection {
    IN,
    OUT,
    INOUT
};

class Pin {
    static constexpr double kStemLength = 15;
    static constexpr double kWireStemWidth = 1;
    static constexpr double kBusStemWidth = 2;
    static constexpr double kTextPadding = 5;

    PinDirection direction;
    std::string name;
    std::string type;
    bool is_bus;
public:
    Pin(std::string _name, PinDirection _direction, bool _is_bus = false, std::string _type = "cc") :
        name(_name), direction(_direction), is_bus(_is_bus), type(_type) { }

    void draw(Cairo::RefPtr<Cairo::Context> ctx, const Cairo::Rectangle& pos) const {
        ctx->save();

        // Draw pin name
        ctx->save();
        if (direction == IN) {
            ctx->move_to(pos.x+kTextPadding, pos.y);
            ctx->show_text(name);
        } else {
            ctx->move_to(pos.x-kTextPadding, pos.y);
            drawRTLText(ctx, name);
        }
        ctx->close_path();
        ctx->restore();

        // Calculate stem alignment
        Cairo::TextExtents extents;
        ctx->get_text_extents(name, extents);

        // Draw pin stem
        ctx->save();
        ctx->set_line_width((is_bus) ? kBusStemWidth : kWireStemWidth);
        ctx->move_to(pos.x, pos.y+extents.y_bearing/2);
        if (direction == IN) {
            ctx->line_to(pos.x-kStemLength, pos.y+extents.y_bearing/2);
        } else {
            ctx->line_to(pos.x+kStemLength, pos.y+extents.y_bearing/2);
        }
        ctx->stroke();
        ctx->restore();

        // Draw pin type
        ctx->save();
        ctx->set_source_rgb(0.5, 0.5, 0.5);
        if (direction == IN) {
            ctx->move_to(pos.x-kTextPadding-kStemLength, pos.y);
            drawRTLText(ctx, type);
        } else {
            ctx->move_to(pos.x+kTextPadding+kStemLength, pos.y);
            ctx->show_text(type);
        }
        ctx->restore();

        ctx->restore();
    }

    PinDirection getDirection() const {
        return direction;
    }

    int innerWidth() const {
        auto surface = Cairo::RecordingSurface::create();
        auto cr = Cairo::Context::create(surface);
        Cairo::TextExtents extents;
        cr->get_text_extents(name, extents);
        return kTextPadding+extents.width;
    }

    int outerWidth() const {
        auto surface = Cairo::RecordingSurface::create();
        auto cr = Cairo::Context::create(surface);
        Cairo::TextExtents extents;
        cr->get_text_extents(type, extents);
        return kStemLength+kTextPadding+extents.width;
    }

    static int height() {
        auto surface = Cairo::RecordingSurface::create();
        auto cr = Cairo::Context::create(surface);
        Cairo::TextExtents extents;
        cr->get_text_extents("Hello world", extents);
        return extents.height;
    }
};

class Section {
    static constexpr double kTextSeparator = 10;
    static constexpr double kTopBottomPadding = 10;
    static constexpr double kBorderThickness = 1.5;
    static constexpr double kPinSpacing = 5;

    std::vector<Pin> pins;
    std::string name;

    int rows() const {
        int left_rows = 0, right_rows = 0;
        for (const auto& pin : pins) {
            if (pin.getDirection() == IN) {
                left_rows += 1;
            } else {
                right_rows += 1;
            }
        }
        return (left_rows > right_rows) ? left_rows : right_rows;
    }
public:
    Section(std::string _name = "") : name(_name) {

    }

    void addPin(const Pin& pin) {
        pins.push_back(pin);
    }

    void draw(Cairo::RefPtr<Cairo::Context> ctx, const Cairo::Rectangle& pos) const {
        ctx->save();

        // Draw section rectangle
        ctx->rectangle(pos.x, pos.y, pos.width, pos.height);
        ctx->stroke();

        std::vector<Pin> left_pins, right_pins;
        std::copy_if (pins.begin(), pins.end(), std::back_inserter(left_pins), [](const Pin& pin){return pin.getDirection() == IN;} );
        std::copy_if (pins.begin(), pins.end(), std::back_inserter(right_pins), [](const Pin& pin){return pin.getDirection() != IN;} );
        Cairo::Rectangle pin_rect = {
            .x = pos.x,
            .y = pos.y+kTopBottomPadding
        };
        for (const auto& pin: left_pins) {
            pin_rect.y += pin.height();
            pin.draw(ctx, pin_rect);
            pin_rect.y += kPinSpacing;
        }
        pin_rect.x = pos.x+pos.width;
        pin_rect.y = pos.y+kTopBottomPadding;
        for (const auto& pin: right_pins) {
            pin_rect.y += pin.height();
            pin.draw(ctx, pin_rect);
            pin_rect.y += kPinSpacing;
        }

        ctx->restore();
    }

    int height() const {
        return kPinSpacing*(rows()-1)+rows()*Pin::height()+2*kTopBottomPadding;
    }

    int minInnerWidth() const {
        int leftInnerWidth = 0, rightInnerWidth = 0;
        for (const auto& pin : pins) {
            if (pin.innerWidth() > leftInnerWidth && pin.getDirection() == IN) {
                leftInnerWidth = pin.innerWidth();
            } else if (pin.innerWidth() > rightInnerWidth && pin.getDirection() == OUT) {
                rightInnerWidth = pin.innerWidth();
            }
        }
        return leftInnerWidth+kTextSeparator+rightInnerWidth;
    }

    int minOuterWidth() const {
        int minOuterWidth = 0;
        for (const auto& pin : pins) {
            if (pin.outerWidth() > minOuterWidth) {
                minOuterWidth = pin.outerWidth();
            }
        }
        return minOuterWidth;
    }
};

class Symbol {
    static constexpr double kNameSpacing = 5;

    std::vector<Section> sections;
    std::string name;
public:
    Symbol(std::string _name) : name(_name) { }

    void addSection(const Section& section) {
        sections.push_back(section);
    }

    void draw(Cairo::RefPtr<Cairo::Context> ctx) const {
        int innerWidth = 0, outerWidth = 0;
        for (const auto& section: sections) {
            if (section.minInnerWidth() > innerWidth) {
                innerWidth = section.minInnerWidth();
            }
            if (section.minOuterWidth() > outerWidth) {
                outerWidth = section.minOuterWidth();
            }
        }

        // Draw symbol name
        ctx->save();
        Cairo::TextExtents extents;
        ctx->get_text_extents(name, extents);
        ctx->move_to(outerWidth+(innerWidth-extents.width)/2, extents.height);
        ctx->show_text(name);
        ctx->restore();

        for (const auto& section: sections) {
            Cairo::Rectangle r = {
                .x = outerWidth,
                .y = extents.height+kNameSpacing,
                .width = innerWidth,
                .height = section.height()
            };
            section.draw(ctx, r);
        }
    }
};

int main()
{
#ifdef CAIRO_HAS_PDF_SURFACE
    std::string filename = "image.pdf";
    int width = 320;
    int height = 320;
    auto surface = Cairo::PdfSurface::create(filename, width, height);
    auto cr = Cairo::Context::create(surface);
    cr->save(); // save the state of the context

    Pin pin1("i_foo", IN, true, "logic [15:0]"),
        pin2("o_bar", OUT, false, "logic"),
        pin3("i_foobar", IN, false, "logic"),
        pin4("i_barfoo", IN, true, "logic [15:0]");
    Section pins;
    pins.addPin(pin1);
    pins.addPin(pin2);
    pins.addPin(pin3);
    pins.addPin(pin4);
    Symbol symbol("My symbol");
    symbol.addSection(pins);
    
    symbol.draw(cr);

    cr->restore();
    cr->show_page();
    std::cout << "Wrote PDF file \"" << filename << "\"" << std::endl;
    return 0;
#else
    std::cout << "You must compile cairo with PDF support for this example to work."
        << std::endl;
    return 1;
#endif
}
