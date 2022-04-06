/*!
 * @file ModelComponent_test.cpp
 *
 * @date Feb 28, 2022
 * @author Tim Spain <timothy.spain@nersc.no>
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "../src/include/ModelComponent.hpp"

#include <stdexcept>

namespace Nextsim {

// (Ab)use the exception mechanism to inform Catch that things are working correctly internally.
class HappyExcept : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Module1 : public ModelComponent {
public:
    Module1() { registerModule(); }
    std::string getName() const override { return "Module1"; }
    void setData(const ModelState& st) override
    {
        throw(HappyExcept(std::string("setData for ") + getName()));
    }
    ModelState getState() const override { return ModelState(); }
    ModelState getState(const OutputLevel& lvl) const override { return getState(); }
    std::set<std::string> uFields() const override { return { "u1" }; }
    std::set<std::string> vFields() const override { return { "v1", "v2" }; }
    std::set<std::string> zFields() const override { return { "z1", "z2", "z3" }; }
};

TEST_CASE("Register a new module", "[ModelComponent]")
{
    Module1 m1;
    REQUIRE_THROWS_AS(ModelComponent::setAllModuleData(ModelState()), HappyExcept);

    std::set<std::string> uu;
    std::set<std::string> vv;
    std::set<std::string> zz;

    ModelComponent::getAllFieldNames(uu, vv, zz);
    REQUIRE(uu.size() == 1);
    REQUIRE(vv.size() == 2);
    REQUIRE(zz.size() == 3);

    ModelComponent::unregisterAllModules();
}

class ModuleSupplyAndWait : public ModelComponent {
public:
    ModuleSupplyAndWait()
        : hice(ModelArray::HField("hice"))
        , p_cice(nullptr)
    {
        registerModule();
        registerProtectedArray(ProtectedArray::H_ICE, &hice);
        requestProtectedArray(ProtectedArray::C_ICE, &p_cice);
    }
    void setData(const ModelState& ms) override { }
    std::string getName() const override { return "SupplyAndWait"; }
    ModelState getState() const override
    {
        return {
            { "hice", hice },
        };
    }
    ModelState getState(const OutputLevel& lvl) const override { return getState(); }

    bool checkNotNull() { return p_cice; }

private:
    HField hice;
    pConstHField p_cice;
};

class ModuleRequestAndSupply : public ModelComponent {
public:
    ModuleRequestAndSupply()
        : cice(ModelArray::HField("cice"))
        , p_hice(nullptr)
    {
        registerModule();
        registerProtectedArray(ProtectedArray::C_ICE, &cice);
        requestProtectedArray(ProtectedArray::H_ICE, &p_hice);
    }
    void setData(const ModelState& ms) override { }
    std::string getName() const override { return "SupplyAndWait"; }
    ModelState getState() const override
    {
        return {
            { "cice", cice },
        };
    }
    ModelState getState(const OutputLevel& lvl) const override { return getState(); }

    bool checkNotNull() { return p_hice; }

private:
    HField cice;
    pConstHField p_hice;
};

TEST_CASE("Test array registration", "[ModelComponent]")
{
    ModuleSupplyAndWait saw;
    ModuleRequestAndSupply ras;

    REQUIRE(ras.checkNotNull());
    REQUIRE(saw.checkNotNull());
}

class ModuleSemiShared: public ModelComponent {
public:
    ModuleSemiShared()
        : qic(ModelArray::HField("qic"))
        , p_qio(nullptr)
    {
        registerModule();
        registerSharedArray(SharedArray::Q_IC, &qic);
        requestProtectedArray(SharedArray::Q_IO, &p_qio);
    }
    void setData(const ModelState& ms) override { }
    std::string getName() const override { return "SemiShared"; }
    ModelState getState() const override
    {
        return {
            { "qic", qic },
        };
    }
    ModelState getState(const OutputLevel& lvl) const override { return getState(); }

    bool checkNotNull() { return p_qio; }

private:
    pConstHField p_qio;
    HField qic;
};

class ModuleShared: public ModelComponent {
public:
    ModuleShared()
        : qio(ModelArray::HField("qio"))
        , p_qic(nullptr)
    {
        registerModule();
        registerSharedArray(SharedArray::Q_IO, &qio);
        requestSharedArray(SharedArray::Q_IC, &p_qic);
    }
    void setData(const ModelState& ms) override { }
    std::string getName() const override { return "Shared"; }
    ModelState getState() const override
    {
        return {
            { "qio", qio },
        };
    }
    ModelState getState(const OutputLevel& lvl) const override { return getState(); }

    bool checkNotNull() { return p_qic; }

private:
    pHField p_qic;
    HField qio;
};

TEST_CASE("Shared and semi-protected arrays", "[ModelComponent]")
{

    ModuleSemiShared semi;
    ModuleShared share;

    REQUIRE(share.checkNotNull());
    REQUIRE(semi.checkNotNull());
}

} /* namespace Nextsim */