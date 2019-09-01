/** @file sky.h  Sky behavior.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2016 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DE_WORLD_SKY_H
#define DE_WORLD_SKY_H

#include <functional>
#include <de/libcore.h>
#include <de/Error>
#include <de/Observers>
#include <de/Vector>
#include <doomsday/defs/ded.h>
#include <doomsday/defs/sky.h>
#include <doomsday/world/Material>
#include "resource/framemodeldef.h"

namespace world {

/**
 * Behavior logic for a sky in the world system.
 */
class Sky : public MapElement
{
public:
    /// Notified when the sky is about to be deleted.
    DE_AUDIENCE(Deletion,            void skyBeingDeleted(Sky const &sky))

    /// Notified whenever the height changes.
    DE_AUDIENCE(HeightChange,        void skyHeightChanged(Sky &sky))

    /// Notified whenever the horizon offset changes.
    DE_AUDIENCE(HorizonOffsetChange, void skyHorizonOffsetChanged(Sky &sky))

    explicit Sky(defn::Sky const *definition = nullptr);

    /**
     * Reconfigure according to the specified @a definition if not @c nullptr, otherwise,
     * reconfigure using the default values.
     *
     * @see configureDefault()
     */
    void configure(defn::Sky const *definition = nullptr);

    /**
     * Reconfigure the sky, returning all values to their defaults.
     *
     * @see configure()
     */
    inline void configureDefault() { configure(); }

    /**
     * Returns the definition used to configure the sky, if any (may return @c nullptr).
     */
    de::Record const *def() const;

    /**
     * Returns the height of the sky as a scale factor [0..1] (@c 1 covers the view).
     *
     * @see setHeight()
     */
    de::dfloat height() const;

    /**
     * Change the height scale factor for the sky.
     *
     * @param newHeight  New height scale factor to apply (will be normalized).
     *
     * @see height()
     */
    void setHeight(de::dfloat newHeight);

    /**
     * Returns the horizon offset for the sky.
     *
     * @see setHorizonOffset()
     */
    de::dfloat horizonOffset() const;

    /**
     * Change the horizon offset for the sky.
     *
     * @param newOffset  New horizon offset to apply.
     *
     * @see horizonOffset()
     */
    void setHorizonOffset(de::dfloat newOffset);

//- Layers ------------------------------------------------------------------------------

    /// Thrown when the required/referenced layer is missing. @ingroup errors
    DE_ERROR(MissingLayerError);

    /**
     * Multiple layers can be used for parallax effects.
     */
    class Layer
    {
    public:
        /// Notified whenever the active-state changes.
        DE_AUDIENCE(ActiveChange,   void skyLayerActiveChanged(Layer &layer))

        /// Notified whenever the masked-state changes.
        DE_AUDIENCE(MaskedChange,   void skyLayerMaskedChanged(Layer &layer))

        /// Notified whenever the layer material changes.
        DE_AUDIENCE(MaterialChange, void skyLayerMaterialChanged(Layer &layer))

    public:
        /**
         * Construct a new sky layer.
         */
        Layer(Sky &sky, Material *material = nullptr);

        /**
         * Returns the sky of which this is a layer.
         */
        Sky &sky() const;

        /**
         * Returns @a true of the layer is currently active.
         *
         * @see setActive()
         */
        bool isActive() const;

        /**
         * Change the 'active' state of the layer. The ActiveChange audience is notified
         * whenever the 'active' state changes.
         *
         * @see isActive()
         */
        void setActive(bool yes);

        inline void enable () { setActive(true);  }
        inline void disable() { setActive(false); }

        /**
         * Returns @c true if the layer's material will be masked.
         *
         * @see setMasked()
         */
        bool isMasked() const;

        /**
         * Change the 'masked' state of the layer. The MaskedChange audience is notified
         * whenever the 'masked' state changes.
         *
         * @see isMasked()
         */
        void setMasked(bool yes);

        /**
         * Returns the material currently assigned to the layer (if any).
         */
        Material *material() const;

        /**
         * Change the material of the layer. The MaterialChange audience is notified
         * whenever the material changes.
         */
        void setMaterial(Material *newMaterial);

        /**
         * Returns the horizontal offset for the layer.
         */
        de::dfloat offset() const;

        /**
         * Change the horizontal offset for the layer.
         *
         * @param newOffset  New offset to apply.
         */
        void setOffset(de::dfloat newOffset);

        /**
         * Returns the fadeout limit for the layer.
         */
        de::dfloat fadeOutLimit() const;

        /**
         * Change the fadeout limit for the layer.
         *
         * @param newLimit  New fadeout limit to apply.
         */
        void setFadeoutLimit(de::dfloat newLimit);

    private:
        DE_PRIVATE(d)
    };

    /**
     * Returns the total number of layers defined for the sky (both active and inactive).
     */
    de::dint layerCount() const;

    /**
     * Returns @c true if @a layerIndex is a known layer index.
     */
    bool hasLayer(de::dint layerIndex) const;

    /**
     * Returns a pointer to the layer associated with @a layerIndex if known; otherwise @c nullptr.
     */
    Layer *layerPtr(de::dint layerIndex) const;

    /**
     * Lookup a layer by it's unique @a layerIndex.
     */
    Layer       &layer(de::dint layerIndex);
    Layer const &layer(de::dint layerIndex) const;

    /**
     * Iterate Layers of the sky.
     *
     * @param callback  Function to call for each Layer.
     */
    de::LoopResult forAllLayers(const std::function<de::LoopResult (Layer &)>& func);
    de::LoopResult forAllLayers(const std::function<de::LoopResult (Layer const &)>& func) const;

#ifdef __CLIENT__
// --------------------------------------------------------------------------------------

    /**
     * Returns the ambient color of the sky. The ambient color is automatically calculated
     * by averaging the color information in the configured layer material textures.
     *
     * Alternatively, this color can be overridden manually by calling @ref setAmbientColor().
     */
    de::Vec3f const &ambientColor() const;

    /**
     * Override the automatically calculated ambient color.
     *
     * @param newColor  New ambient color to apply (will be normalized).
     *
     * @see ambientColor()
     */
    void setAmbientColor(de::Vec3f const &newColor);

#endif // __CLIENT__

protected:
    de::dint property(world::DmuArgs &args) const;
    de::dint setProperty(world::DmuArgs const &args);

private:
    DE_PRIVATE(d)
};

typedef Sky::Layer SkyLayer;

} // namespace world

#endif // DE_WORLD_SKY_H
