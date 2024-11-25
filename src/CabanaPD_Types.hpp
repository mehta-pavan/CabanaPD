/****************************************************************************
 * Copyright (c) 2022 by Oak Ridge National Laboratory                      *
 * All rights reserved.                                                     *
 *                                                                          *
 * This file is part of CabanaPD. CabanaPD is distributed under a           *
 * BSD 3-clause license. For the licensing terms see the LICENSE file in    *
 * the top-level directory.                                                 *
 *                                                                          *
 * SPDX-License-Identifier: BSD-3-Clause                                    *
 ****************************************************************************/

#ifndef TYPES_H
#define TYPES_H

namespace CabanaPD
{
// Fracture tags.
struct Elastic
{
};
struct Fracture
{
};

// Contact and DEM (contact without PD) tags.

struct Contact
{
};
struct NoContact
{
};
template <class>
struct is_contact : public std::false_type
{
};

// Thermal tags.
struct TemperatureIndependent
{
    using base_type = TemperatureIndependent;
};
struct TemperatureDependent
{
    using base_type = TemperatureDependent;
};
struct DynamicTemperature : public TemperatureDependent
{
    using base_type = TemperatureDependent;
};

//! Static type checkers.
template <class>
struct is_temperature_dependent : public std::false_type
{
};
template <>
struct is_temperature_dependent<TemperatureDependent> : public std::true_type
{
};
template <>
struct is_temperature_dependent<DynamicTemperature> : public std::true_type
{
};
template <class>
struct is_heat_transfer : public std::false_type
{
};
template <>
struct is_heat_transfer<DynamicTemperature> : public std::true_type
{
};

// Force model tags.
struct PMB
{
};
struct LinearPMB
{
};
struct LPS
{
};
struct LinearLPS
{
};

} // namespace CabanaPD
#endif
