/** @file baseguiapp.cpp  Base class for GUI applications.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/BaseGuiApp"
#include "de/VRConfig"

#include <de/ArrayValue>
#include <de/CommandLine>
#include <de/Config>
#include <de/ConstantRule>
#include <de/DictionaryValue>
#include <de/FileSystem>
#include <de/Function>
#include <de/NativeFont>
#include <de/BaseWindow>
#include <de/ScriptSystem>
#include <QFontDatabase>

#ifdef WIN32
#  define CONST const
#  include <d2d1.h>
#endif

namespace de {

static Value *Function_App_LoadFont(Context &, Function::ArgumentValues const &args)
{
    try
    {
        // Try to load the specific font.
        Block data(App::rootFolder().locate<File const>(args.at(0)->asText()));
        int id;
        id = QFontDatabase::addApplicationFontFromData(data);
        if (id < 0)
        {
            LOG_RES_WARNING("Failed to load font:");
        }
        else
        {
            LOG_RES_VERBOSE("Loaded font: %s") << args.at(0)->asText();
            //qDebug() << args.at(0)->asText();
            //qDebug() << "Families:" << QFontDatabase::applicationFontFamilies(id);
        }
    }
    catch (Error const &er)
    {
        LOG_RES_WARNING("Failed to load font:\n") << er.asText();
    }
    return 0;
}

static Value *Function_App_AddFontMapping(Context &, Function::ArgumentValues const &args)
{
    // arg 0: family name
    // arg 1: dictionary with [Text style, Number weight] => Text fontname

    // styles: regular, italic
    // weight: 0-99 (25=light, 50=normal, 75=bold)

    NativeFont::StyleMapping mapping;

    DictionaryValue const &dict = args.at(1)->as<DictionaryValue>();
    DENG2_FOR_EACH_CONST(DictionaryValue::Elements, i, dict.elements())
    {
        NativeFont::Spec spec;
        ArrayValue const &key = i->first.value->as<ArrayValue>();
        if (key.at(0).asText() == "italic")
        {
            spec.style = NativeFont::Italic;
        }
        spec.weight = roundi(key.at(1).asNumber());
        mapping.insert(spec, i->second->asText());
    }

    NativeFont::defineMapping(args.at(0)->asText(), mapping);

    return 0;
}

DENG2_PIMPL(BaseGuiApp)
, DENG2_OBSERVES(Variable, Change)
{
    Binder binder;
    QScopedPointer<PersistentState> uiState;
    GLShaderBank shaders;
    WaveformBank waveforms;
    VRConfig vr;
    float windowPixelRatio = 1.0f; ///< Without user's Config.ui.scaleConfig
    ConstantRule *pixelRatio = new ConstantRule;

    Impl(Public *i) : Base(i)
    {
#if defined(WIN32)
        // Use the Direct2D API to find out the desktop pixel ratio.
        ID2D1Factory *d2dFactory = nullptr;
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        if (SUCCEEDED(hr))
        {
            FLOAT dpiX = 96;
            FLOAT dpiY = 96;
            d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
            windowPixelRatio = dpiX / 96.0;
            d2dFactory->Release();
            d2dFactory = nullptr;
        }
#endif
    }

    ~Impl() override
    {
        releaseRef(pixelRatio);
    }

    void variableValueChanged(Variable &, const Value &) override
    {
        self().setPixelRatio(windowPixelRatio);
    }
};

BaseGuiApp::BaseGuiApp(int &argc, char **argv)
    : GuiApp(argc, argv), d(new Impl(this))
{
    d->binder.init(scriptSystem()["App"])
            << DENG2_FUNC (App_AddFontMapping, "addFontMapping", "family" << "mappings")
            << DENG2_FUNC (App_LoadFont,       "loadFont", "fileName");
}

void BaseGuiApp::glDeinit()
{
    GLWindow::glActiveMain();

    d->vr.oculusRift().deinit();
    d->shaders.clear();
}

void BaseGuiApp::initSubsystems(SubsystemInitFlags flags)
{
    GuiApp::initSubsystems(flags);

#if !defined(WIN32)
    d->windowPixelRatio = float(devicePixelRatio());
#endif
    // The "-dpi" option overrides the detected pixel ratio.
    if (auto dpi = commandLine().check("-dpi", 1))
    {
        d->windowPixelRatio = dpi.params.at(0).toFloat();
    }
    setPixelRatio(d->windowPixelRatio);

    Config::get("ui.scaleFactor").audienceForChange() += d;

    d->uiState.reset(new PersistentState("UIState"));
}

const Rule &BaseGuiApp::pixelRatio() const
{
    return *d->pixelRatio;
}

void BaseGuiApp::setPixelRatio(float pixelRatio)
{
    d->windowPixelRatio = pixelRatio;

    // Apply the overall UI scale factor.
    pixelRatio *= config().getf("ui.scaleFactor", 1.0f);

    if (!fequal(d->pixelRatio->value(), pixelRatio))
    {
        LOG_VERBOSE("Pixel ratio changed to %.1f") << pixelRatio;

        d->pixelRatio->set(pixelRatio);
        scriptSystem()["DisplayMode"].set("PIXEL_RATIO", Value::Number(pixelRatio));
    }
}

BaseGuiApp &BaseGuiApp::app()
{
    return static_cast<BaseGuiApp &>(App::app());
}

PersistentState &BaseGuiApp::persistentUIState()
{
    return *app().d->uiState;
}

GLShaderBank &BaseGuiApp::shaders()
{
    return app().d->shaders;
}

WaveformBank &BaseGuiApp::waveforms()
{
    return app().d->waveforms;
}

VRConfig &BaseGuiApp::vr()
{
    return app().d->vr;
}

void BaseGuiApp::beginNativeUIMode()
{
#if !defined (DENG_MOBILE)
    // Switch temporarily to windowed mode. Not needed on macOS because the display mode
    // is never changed on that platform.
    #if !defined (MACOSX)
    {
        auto &win = static_cast<BaseWindow &>(GLWindow::main());
        win.saveState();
        int const windowedMode[] = {
            BaseWindow::Fullscreen, false,
            BaseWindow::End
        };
        win.changeAttributes(windowedMode);
    }
    #endif
#endif
}

void BaseGuiApp::endNativeUIMode()
{
#if !defined (DENG_MOBILE)
#   if !defined (MACOSX)
    {
        static_cast<BaseWindow &>(GLWindow::main()).restoreState();
    }
#   endif
#endif
}
    

} // namespace de
