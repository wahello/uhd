//
// Copyright 2014-15 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "dboard_ctor_args.hpp"
#include "twinrx/twinrx_ctrl.hpp"
#include "twinrx/twinrx_experts.hpp"
#include "twinrx/twinrx_ids.hpp"
#include "twinrx/twinrx_io.hpp"
#include <uhd/experts/expert_factory.hpp>
#include <uhd/types/device_addr.hpp>
#include <uhd/types/ranges.hpp>
#include <uhd/types/sensors.hpp>
#include <uhd/usrp/dboard_base.hpp>
#include <uhd/usrp/dboard_manager.hpp>
#include <uhd/utils/log.hpp>
#include <uhd/utils/static.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <memory>
//#include <fstream>    //Needed for _expert->to_dot() below

using namespace uhd;
using namespace uhd::usrp;
using namespace uhd::usrp::dboard::twinrx;
using namespace uhd::experts;

/*!
 * twinrx_rcvr_fe is the dbaord class (dboard_base) that
 * represents each front-end of a TwinRX board. UHD will
 * create and hold two instances of this class per TwinRX
 * dboard.
 *
 */
class twinrx_rcvr_fe : public rx_dboard_base
{
public:
    twinrx_rcvr_fe(
        ctor_args_t args, expert_container::sptr expert, twinrx_ctrl::sptr ctrl)
        : rx_dboard_base(args)
        , _expert(expert)
        , _ctrl(ctrl)
        , _ch_name(dboard_ctor_args_t::cast(args).sd_name)
    {
        //---------------------------------------------------------
        // Add user-visible, channel specific properties to front-end tree
        //---------------------------------------------------------

        // Generic
        get_rx_subtree()->create<std::string>("name").set("TwinRX RX" + _ch_name);
        get_rx_subtree()->create<bool>("use_lo_offset").set(false);
        get_rx_subtree()
            ->create<std::string>("connection")
            .set(_ch_name == "0" ? "II" : "QQ"); // Ch->ADC port mapping
        static const double BW = 80e6;
        get_rx_subtree()
            ->create<meta_range_t>("bandwidth/range")
            .set(freq_range_t(BW, BW));
        get_rx_subtree()
            ->create<double>("bandwidth/value")
            .set_coercer([this](const double bandwidth) {
                return get_rx_subtree()
                    ->access<meta_range_t>("bandwidth/range")
                    .get()
                    .clip(bandwidth);
            })
            .set(BW);

        // Command Time
        expert_factory::add_data_node<time_spec_t>(
            _expert, prepend_ch("time/rx_frontend", _ch_name), time_spec_t(0.0));
        expert_factory::add_prop_node<time_spec_t>(_expert,
            get_rx_subtree(),
            "time/cmd",
            prepend_ch("time/cmd", _ch_name),
            time_spec_t(0.0));

        // Frequency Specific
        get_rx_subtree()
            ->create<meta_range_t>("freq/range")
            .set(freq_range_t(10e6, 6.0e9));
        expert_factory::add_dual_prop_node<double>(_expert,
            get_rx_subtree(),
            "freq/value",
            prepend_ch("freq/desired", _ch_name),
            prepend_ch("freq/coerced", _ch_name),
            1.0e9,
            AUTO_RESOLVE_ON_READ_WRITE);
        get_rx_subtree()->create<device_addr_t>("tune_args").set(device_addr_t());

        static const double DEFAULT_IF_FREQ = 150e6;
        meta_range_t if_freq_range;
        if_freq_range.push_back(
            range_t(-DEFAULT_IF_FREQ - (BW / 2), -DEFAULT_IF_FREQ + (BW / 2)));
        if_freq_range.push_back(
            range_t(DEFAULT_IF_FREQ - (BW / 2), DEFAULT_IF_FREQ + (BW / 2)));
        get_rx_subtree()->create<meta_range_t>("if_freq/range").set(if_freq_range);
        expert_factory::add_dual_prop_node<double>(_expert,
            get_rx_subtree(),
            "if_freq/value",
            prepend_ch("if_freq/desired", _ch_name),
            prepend_ch("if_freq/coerced", _ch_name),
            DEFAULT_IF_FREQ,
            AUTO_RESOLVE_ON_WRITE);

        // LO Specific
        get_rx_subtree()
            ->create<meta_range_t>("los/LO1/freq/range")
            .set(freq_range_t(2.0e9, 6.8e9));
        expert_factory::add_dual_prop_node<double>(_expert,
            get_rx_subtree(),
            "los/LO1/freq/value",
            prepend_ch("los/LO1/freq/desired", _ch_name),
            prepend_ch("los/LO1/freq/coerced", _ch_name),
            0.0,
            AUTO_RESOLVE_ON_READ_WRITE);
        get_rx_subtree()
            ->create<meta_range_t>("los/LO2/freq/range")
            .set(freq_range_t(1.0e9, 3.0e9));
        expert_factory::add_dual_prop_node<double>(_expert,
            get_rx_subtree(),
            "los/LO2/freq/value",
            prepend_ch("los/LO2/freq/desired", _ch_name),
            prepend_ch("los/LO2/freq/coerced", _ch_name),
            0.0,
            AUTO_RESOLVE_ON_READ_WRITE);
        get_rx_subtree()
            ->create<std::vector<std::string>>("los/all/source/options")
            .set({"internal", "external", "companion", "disabled", "reimport"});
        expert_factory::add_prop_node<std::string>(_expert,
            get_rx_subtree(),
            "los/all/source/value",
            prepend_ch("los/all/source", _ch_name),
            "internal",
            AUTO_RESOLVE_ON_WRITE);
        expert_factory::add_prop_node<bool>(_expert,
            get_rx_subtree(),
            "los/all/export",
            prepend_ch("los/all/export", _ch_name),
            false,
            AUTO_RESOLVE_ON_WRITE);

        // LO1 Charge Pump
        get_rx_subtree()
            ->create<meta_range_t>("los/LO1/charge_pump/range")
            .set(_ctrl->get_lo1_charge_pump_range());
        expert_factory::add_dual_prop_node<double>(_expert,
            get_rx_subtree(),
            "los/LO1/charge_pump/value",
            prepend_ch("los/LO1/charge_pump/desired", _ch_name),
            prepend_ch("los/LO1/charge_pump/coerced", _ch_name),
            (get_rx_id() == twinrx::TWINRX_REV_C_ID) ? 0.9e-6 : 0.9375e-6,
            AUTO_RESOLVE_ON_READ_WRITE);

        // LO2 Charge Pump
        get_rx_subtree()
            ->create<meta_range_t>("los/LO2/charge_pump/range")
            .set(_ctrl->get_lo2_charge_pump_range());
        expert_factory::add_dual_prop_node<double>(_expert,
            get_rx_subtree(),
            "los/LO2/charge_pump/value",
            prepend_ch("los/LO2/charge_pump/desired", _ch_name),
            prepend_ch("los/LO2/charge_pump/coerced", _ch_name),
            1.25e-6,
            AUTO_RESOLVE_ON_READ_WRITE);

        // Gain Specific
        get_rx_subtree()
            ->create<meta_range_t>("gains/all/range")
            .set(gain_range_t(0, 93, double(1.0)));
        expert_factory::add_prop_node<double>(_expert,
            get_rx_subtree(),
            "gains/all/value",
            prepend_ch("gain", _ch_name),
            0.0,
            AUTO_RESOLVE_ON_WRITE);
        get_rx_subtree()
            ->create<std::vector<std::string>>("gains/all/profile/options")
            .set({"low-noise", "low-distortion", "default"});
        expert_factory::add_prop_node<std::string>(_expert,
            get_rx_subtree(),
            "gains/all/profile/value",
            prepend_ch("gain_profile", _ch_name),
            "default",
            AUTO_RESOLVE_ON_WRITE);

        // Antenna Specific
        get_rx_subtree()
            ->create<std::vector<std::string>>("antenna/options")
            .set({"RX1", "RX2"});
        expert_factory::add_prop_node<std::string>(_expert,
            get_rx_subtree(),
            "antenna/value",
            prepend_ch("antenna", _ch_name),
            (_ch_name == "0" ? "RX1" : "RX2"),
            AUTO_RESOLVE_ON_WRITE);
        expert_factory::add_prop_node<bool>(_expert,
            get_rx_subtree(),
            "enabled",
            prepend_ch("enabled", _ch_name),
            false,
            AUTO_RESOLVE_ON_WRITE);
        // ID for cal data lookup. This depends on how many channels are enabled
        expert_factory::add_prop_node<std::string>(
            _expert, get_rx_subtree(), "id", prepend_ch("id", _ch_name), "twinrx");

        // Readback
        get_rx_subtree()
            ->create<sensor_value_t>("sensors/lo_locked")
            .set_publisher([this]() { return this->get_lo_locked(); });

        //---------------------------------------------------------
        // Add internal channel-specific data nodes to expert
        //---------------------------------------------------------
        expert_factory::add_data_node<lo_inj_side_t>(
            _expert, prepend_ch("ch/LO1/inj_side", _ch_name), INJ_LOW_SIDE);
        expert_factory::add_data_node<lo_inj_side_t>(
            _expert, prepend_ch("ch/LO2/inj_side", _ch_name), INJ_LOW_SIDE);
        expert_factory::add_data_node<twinrx_ctrl::signal_path_t>(
            _expert, prepend_ch("ch/signal_path", _ch_name), twinrx_ctrl::PATH_LOWBAND);
        expert_factory::add_data_node<twinrx_ctrl::preselector_path_t>(
            _expert, prepend_ch("ch/lb_presel", _ch_name), twinrx_ctrl::PRESEL_PATH1);
        expert_factory::add_data_node<twinrx_ctrl::preselector_path_t>(
            _expert, prepend_ch("ch/hb_presel", _ch_name), twinrx_ctrl::PRESEL_PATH1);
        expert_factory::add_data_node<bool>(
            _expert, prepend_ch("ch/lb_preamp_presel", _ch_name), false);
        expert_factory::add_data_node<bool>(
            _expert, prepend_ch("ant/lb_preamp_presel", _ch_name), false);
        expert_factory::add_data_node<twinrx_ctrl::preamp_state_t>(
            _expert, prepend_ch("ch/preamp1", _ch_name), twinrx_ctrl::PREAMP_BYPASS);
        expert_factory::add_data_node<twinrx_ctrl::preamp_state_t>(
            _expert, prepend_ch("ant/preamp1", _ch_name), twinrx_ctrl::PREAMP_BYPASS);
        expert_factory::add_data_node<bool>(
            _expert, prepend_ch("ch/preamp2", _ch_name), false);
        expert_factory::add_data_node<bool>(
            _expert, prepend_ch("ant/preamp2", _ch_name), false);
        expert_factory::add_data_node<uint8_t>(
            _expert, prepend_ch("ch/input_atten", _ch_name), 0);
        expert_factory::add_data_node<uint8_t>(
            _expert, prepend_ch("ant/input_atten", _ch_name), 0);
        expert_factory::add_data_node<uint8_t>(
            _expert, prepend_ch("ch/lb_atten", _ch_name), 0);
        expert_factory::add_data_node<uint8_t>(
            _expert, prepend_ch("ch/hb_atten", _ch_name), 0);
        expert_factory::add_data_node<twinrx_ctrl::lo_source_t>(
            _expert, prepend_ch("ch/LO1/source", _ch_name), twinrx_ctrl::LO_INTERNAL);
        expert_factory::add_data_node<twinrx_ctrl::lo_source_t>(
            _expert, prepend_ch("ch/LO2/source", _ch_name), twinrx_ctrl::LO_INTERNAL);
        expert_factory::add_data_node<lo_synth_mapping_t>(
            _expert, prepend_ch("synth/LO1/mapping", _ch_name), MAPPING_NONE);
        expert_factory::add_data_node<lo_synth_mapping_t>(
            _expert, prepend_ch("synth/LO2/mapping", _ch_name), MAPPING_NONE);
    }

    ~twinrx_rcvr_fe(void) override {}

    sensor_value_t get_lo_locked()
    {
        bool locked               = true;
        twinrx_ctrl::channel_t ch = (_ch_name == "0") ? twinrx_ctrl::CH1
                                                      : twinrx_ctrl::CH2;
        locked &= _ctrl->read_lo1_locked(ch);
        locked &= _ctrl->read_lo2_locked(ch);
        return sensor_value_t("LO", locked, "locked", "unlocked");
    }

private:
    expert_container::sptr _expert;
    twinrx_ctrl::sptr _ctrl;
    const std::string _ch_name;
};

/*!
 * twinrx_rcvr is the top-level container for each
 * TwinRX board. UHD will hold one instance of this
 * class per TwinRX dboard. This class is responsible
 * for owning all the control classes for the board.
 *
 */
class twinrx_rcvr : public rx_dboard_base
{
public:
    typedef std::shared_ptr<twinrx_rcvr> sptr;

    twinrx_rcvr(ctor_args_t args) : rx_dboard_base(args)
    {
        _db_iface                          = get_iface();
        twinrx_gpio::sptr gpio_iface       = std::make_shared<twinrx_gpio>(_db_iface);
        twinrx_cpld_regmap::sptr cpld_regs = std::make_shared<twinrx_cpld_regmap>();
        cpld_regs->initialize(*gpio_iface, false);
        _ctrl   = twinrx_ctrl::make(_db_iface, gpio_iface, cpld_regs, get_rx_id());
        _expert = expert_factory::create_container("twinrx_expert");
    }

    ~twinrx_rcvr(void) override {}

    inline expert_container::sptr get_expert()
    {
        return _expert;
    }

    inline twinrx_ctrl::sptr get_ctrl()
    {
        return _ctrl;
    }

    void initialize() override
    {
        //---------------------------------------------------------
        // Add internal channel-agnostic data nodes to expert
        //---------------------------------------------------------
        expert_factory::add_data_node<twinrx_ctrl::lo_export_source_t>(
            _expert, "com/LO1/export_source", twinrx_ctrl::LO_EXPORT_DISABLED);
        expert_factory::add_data_node<twinrx_ctrl::lo_export_source_t>(
            _expert, "com/LO2/export_source", twinrx_ctrl::LO_EXPORT_DISABLED);
        expert_factory::add_data_node<twinrx_ctrl::antenna_mapping_t>(
            _expert, "com/ant_mapping", twinrx_ctrl::ANTX_NATIVE);
        expert_factory::add_data_node<twinrx_ctrl::cal_mode_t>(
            _expert, "com/cal_mode", twinrx_ctrl::CAL_DISABLED);
        expert_factory::add_data_node<bool>(
            _expert, "com/synth/LO1/hopping_enabled", false);
        expert_factory::add_data_node<bool>(
            _expert, "com/synth/LO2/hopping_enabled", false);

        //---------------------------------------------------------
        // Add workers to expert
        //---------------------------------------------------------
        // Channel (front-end) specific
        for (const std::string& fe : _fe_names) {
            expert_factory::add_worker_node<twinrx_freq_path_expert>(
                _expert, _expert->node_retriever(), fe);
            expert_factory::add_worker_node<twinrx_freq_coercion_expert>(
                _expert, _expert->node_retriever(), fe);
            expert_factory::add_worker_node<twinrx_chan_gain_expert>(
                _expert, _expert->node_retriever(), fe);
            expert_factory::add_worker_node<twinrx_scheduling_expert>(
                _expert, _expert->node_retriever(), fe);
            expert_factory::add_worker_node<twinrx_nyquist_expert>(
                _expert, _expert->node_retriever(), fe, _db_iface);
        }

        // Channel (front-end) agnostic
        expert_factory::add_worker_node<twinrx_lo_config_expert>(
            _expert, _expert->node_retriever());
        expert_factory::add_worker_node<twinrx_lo_mapping_expert>(
            _expert, _expert->node_retriever(), STAGE_LO1);
        expert_factory::add_worker_node<twinrx_lo_mapping_expert>(
            _expert, _expert->node_retriever(), STAGE_LO2);
        expert_factory::add_worker_node<twinrx_antenna_expert>(
            _expert, _expert->node_retriever());
        expert_factory::add_worker_node<twinrx_ant_gain_expert>(
            _expert, _expert->node_retriever());
        expert_factory::add_worker_node<twinrx_settings_expert>(
            _expert, _expert->node_retriever(), _ctrl);

        /*//Expert debug code
        std::ofstream dot_file("/tmp/twinrx.dot", std::ios::out);
        dot_file << _expert->to_dot();
        dot_file.close();
        */

        _expert->debug_audit();
        _expert->resolve_all(true);
    }

    static dboard_base::sptr make_twinrx_fe(dboard_base::ctor_args_t args)
    {
        const dboard_ctor_args_t& db_args = dboard_ctor_args_t::cast(args);
        sptr container = std::dynamic_pointer_cast<twinrx_rcvr>(db_args.rx_container);
        if (container) {
            dboard_base::sptr fe = dboard_base::sptr(
                new twinrx_rcvr_fe(args, container->get_expert(), container->get_ctrl()));
            container->add_twinrx_fe(db_args.sd_name);
            return fe;
        } else {
            throw uhd::assertion_error("error creating twinrx frontend");
        }
    }

protected:
    inline void add_twinrx_fe(const std::string& name)
    {
        _fe_names.push_back(name);
    }

private:
    typedef std::map<std::string, dboard_base::sptr> twinrx_fe_map_t;

    dboard_iface::sptr _db_iface;
    twinrx_ctrl::sptr _ctrl;
    std::vector<std::string> _fe_names;
    expert_container::sptr _expert;
};

/*!
 * Initialization Sequence for each TwinRX board:
 * - make_twinrx_container is called which creates an instance of twinrx_rcvr
 * - twinrx_rcvr::make_twinrx_fe is called with channel "0" which creates an instance of
 * twinrx_rcvr_fe
 * - twinrx_rcvr::make_twinrx_fe is called with channel "1" which creates an instance of
 * twinrx_rcvr_fe
 * - twinrx_rcvr::initialize is called with finishes the init sequence
 *
 */
static dboard_base::sptr make_twinrx_container(dboard_base::ctor_args_t args)
{
    return dboard_base::sptr(new twinrx_rcvr(args));
}

UHD_STATIC_BLOCK(reg_twinrx_dboards)
{
    dboard_manager::register_dboard_restricted(twinrx::TWINRX_REV_A_ID,
        &twinrx_rcvr::make_twinrx_fe,
        "TwinRX Rev A",
        {"0", "1"},
        &make_twinrx_container);

    dboard_manager::register_dboard_restricted(twinrx::TWINRX_REV_B_ID,
        &twinrx_rcvr::make_twinrx_fe,
        "TwinRX Rev B",
        {"0", "1"},
        &make_twinrx_container);

    dboard_manager::register_dboard_restricted(twinrx::TWINRX_REV_C_ID,
        &twinrx_rcvr::make_twinrx_fe,
        "TwinRX Rev C",
        {"0", "1"},
        &make_twinrx_container);
}
