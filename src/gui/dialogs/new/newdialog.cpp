#include "newdialog.h"

#include "helper/async_modal.h"
#include "machine/simulator_exception.h"
#include "mainwindow/mainwindow.h"

#include <utility>

#ifdef __EMSCRIPTEN__
    #include "qhtml5file.h"

    #include <QFileInfo>
#endif

NewDialog::NewDialog(QWidget *parent, QSettings *settings) : QDialog(parent) {
    setWindowTitle("New machine");

    this->settings = settings;
    config.reset();

    ui.reset(new Ui::NewDialog());
    ui->setupUi(this);
    ui_cache_p.reset(new Ui::NewDialogCache());
    ui_cache_p->setupUi(ui->tab_cache_program);
    ui_cache_p->writeback_policy->hide();
    ui_cache_p->label_writeback->hide();
    ui_cache_d.reset(new Ui::NewDialogCache());
    ui_cache_d->setupUi(ui->tab_cache_data);
    ui_cache_l2.reset(new Ui::NewDialogCache());
    ui_cache_l2->setupUi(ui->tab_cache_level2);

    connect(
        ui->pushButton_example, &QAbstractButton::clicked, this,
        &NewDialog::create_example);
    connect(
        ui->pushButton_start_empty, &QAbstractButton::clicked, this,
        &NewDialog::create_empty);
    connect(
        ui->pushButton_load, &QAbstractButton::clicked, this,
        &NewDialog::create);
    connect(
        ui->pushButton_cancel, &QAbstractButton::clicked, this,
        &NewDialog::cancel);
    connect(
        ui->pushButton_browse, &QAbstractButton::clicked, this,
        &NewDialog::browse_elf);
    connect(
        ui->load_code_from_dwarf, &QAbstractButton::clicked, this,
        &NewDialog::set_load_code_from);
    connect(
        ui->load_code_from_local, &QAbstractButton::clicked, this,
        &NewDialog::set_load_code_from);
    connect(
        ui->elf_file, &QLineEdit::textChanged, this, &NewDialog::elf_change);
    connect(
        ui->preset_no_pipeline, &QAbstractButton::toggled, this,
        &NewDialog::set_preset);
    connect(
        ui->preset_no_pipeline_cache, &QAbstractButton::toggled, this,
        &NewDialog::set_preset);
    connect(
        ui->preset_pipelined_bare, &QAbstractButton::toggled, this,
        &NewDialog::set_preset);
    connect(
        ui->preset_pipelined, &QAbstractButton::toggled, this,
        &NewDialog::set_preset);
    connect(
        ui->reset_at_compile, &QAbstractButton::clicked, this,
        &NewDialog::reset_at_compile_change);

    connect(
        ui->xlen_64bit, &QAbstractButton::clicked, this,
        &NewDialog::xlen_64bit_change);
    connect(
        ui->isa_atomic, &QAbstractButton::clicked, this,
        &NewDialog::isa_atomic_change);
    connect(
        ui->isa_multiply, &QAbstractButton::clicked, this,
        &NewDialog::isa_multiply_change);
    connect(
        ui->pipelined, &QAbstractButton::clicked, this,
        &NewDialog::pipelined_change);
    connect(
        ui->delay_slot, &QAbstractButton::clicked, this,
        &NewDialog::delay_slot_change);
    connect(
        ui->hazard_unit, &QGroupBox::clicked, this,
        &NewDialog::hazard_unit_change);
    connect(
        ui->hazard_stall, &QAbstractButton::clicked, this,
        &NewDialog::hazard_unit_change);
    connect(
        ui->hazard_stall_forward, &QAbstractButton::clicked, this,
        &NewDialog::hazard_unit_change);

    connect(
        ui->mem_protec_exec, &QAbstractButton::clicked, this,
        &NewDialog::mem_protec_exec_change);
    connect(
        ui->mem_protec_write, &QAbstractButton::clicked, this,
        &NewDialog::mem_protec_write_change);
    connect(
        ui->mem_time_read, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &NewDialog::mem_time_read_change);
    connect(
        ui->mem_time_write, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &NewDialog::mem_time_write_change);
    connect(
        ui->mem_enable_burst, &QAbstractButton::clicked, this,
        &NewDialog::mem_enable_burst_change);
    connect(
        ui->mem_time_burst, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &NewDialog::mem_time_burst_change);
    connect(
        ui->mem_time_level2, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &NewDialog::mem_time_level2_change);

    connect(
        ui->osemu_enable, &QAbstractButton::clicked, this,
        &NewDialog::osemu_enable_change);
    connect(
        ui->osemu_known_syscall_stop, &QAbstractButton::clicked, this,
        &NewDialog::osemu_known_syscall_stop_change);
    connect(
        ui->osemu_unknown_syscall_stop, &QAbstractButton::clicked, this,
        &NewDialog::osemu_unknown_syscall_stop_change);
    connect(
        ui->osemu_interrupt_stop, &QAbstractButton::clicked, this,
        &NewDialog::osemu_interrupt_stop_change);
    connect(
        ui->osemu_exception_stop, &QAbstractButton::clicked, this,
        &NewDialog::osemu_exception_stop_change);
    connect(
        ui->osemu_fs_root_browse, &QAbstractButton::clicked, this,
        &NewDialog::browse_osemu_fs_root);
    connect(ui->osemu_fs_root, &QLineEdit::textChanged, this, &NewDialog::osemu_fs_root_change);

    cache_handler_d = new NewDialogCacheHandler(this, ui_cache_d.data());
    cache_handler_p = new NewDialogCacheHandler(this, ui_cache_p.data());
    cache_handler_l2 = new NewDialogCacheHandler(this, ui_cache_l2.data());

    // TODO remove this block when protections are implemented
    ui->mem_protec_exec->setVisible(false);
    ui->mem_protec_write->setVisible(false);

    load_settings(); // Also configures gui
}

void NewDialog::switch2custom() {
    ui->preset_custom->setChecked(true);
    config_gui();
}

void NewDialog::closeEvent(QCloseEvent *) {
    load_settings(); // Reset from settings
    // Close the main window if not already configured
    auto *prnt = (MainWindow *)parent();
    if (!prnt->configured()) {
        prnt->close();
    }
}

void NewDialog::cancel() {
    this->close();
}

void NewDialog::create() {
    auto *p_window = (MainWindow *)parent();

    try {
        p_window->create_core(*config, true, false);
    } catch (const machine::SimulatorExceptionInput &e) {
        showAsyncCriticalBox(
            this, "Error while initializing new machine", e.msg(false), e.msg(true),
            "Please check that ELF executable really exists and is in correct format.");
        return;
    }

    store_settings(); // Save to settings
    this->close();
}

void NewDialog::create_empty() {
    auto *p_window = (MainWindow *)parent();
    p_window->create_core(*config, false, true);
    store_settings(); // Save to settings
    this->close();
}

void NewDialog::create_example() {
    auto *p_window = (MainWindow *)parent();
    QString example(":/samples/template.S");
    p_window->create_core(*config, false, true);
    store_settings(); // Save to settings
    p_window->close_source_by_name(example, false);
    p_window->example_source(example);
    p_window->show_program();
    p_window->compile_source();
    this->close();
}

void NewDialog::browse_elf() {
#ifndef __EMSCRIPTEN__
    QFileDialog elf_dialog(this);
    elf_dialog.setFileMode(QFileDialog::ExistingFile);
    if (elf_dialog.exec()) {
        QString path = elf_dialog.selectedFiles()[0];
        ui->elf_file->setText(path);
        config->set_elf(path);
    }
    // Elf shouldn't have any other effect, so we skip config_gui here
#else
    QHtml5File::load("*", [&](const QByteArray &content, const QString &fileName) {
        QFileInfo fi(fileName);
        QString elf_name = fi.fileName();
        QFile file(elf_name);
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        file.write(content);
        file.close();
        ui->elf_file->setText(elf_name);
        config->set_elf(elf_name);
    });
#endif
}

void NewDialog::elf_change(QString val) {
    config->set_elf(std::move(val));
}

void NewDialog::set_preset() {
    unsigned pres_n = preset_number();
    if (pres_n > 0) {
        config->preset((enum machine::ConfigPresets)(pres_n - 1));
        config_gui();
    }
}

void NewDialog::set_load_code_from() {
    unsigned pres_n = load_code_from_number();
    printf("onclick pres_n: %d\n", pres_n);
    if (pres_n > 0) {
        config->set_load_code_from((enum machine::ConfigLoadCodeFrom)(pres_n - 1));
        config_gui();
    }
}

void NewDialog::xlen_64bit_change(bool val) {
    if (val)
        config->set_simulated_xlen(machine::Xlen::_64);
    else
        config->set_simulated_xlen(machine::Xlen::_32);
    switch2custom();
}

void NewDialog::isa_atomic_change(bool val) {
    auto isa_mask =  machine::ConfigIsaWord::byChar('A');
    if (val)
        config->modify_isa_word(isa_mask, isa_mask);
    else
        config->modify_isa_word(isa_mask, machine::ConfigIsaWord::empty());
    switch2custom();
}

void NewDialog::isa_multiply_change(bool val) {
    auto isa_mask =  machine::ConfigIsaWord::byChar('M');
    if (val)
        config->modify_isa_word(isa_mask, isa_mask);
    else
        config->modify_isa_word(isa_mask, machine::ConfigIsaWord::empty());
    switch2custom();
}

void NewDialog::pipelined_change(bool val) {
    config->set_pipelined(val);
    switch2custom();
}

void NewDialog::delay_slot_change(bool val) {
    config->set_delay_slot(val);
    switch2custom();
}

void NewDialog::hazard_unit_change() {
    if (ui->hazard_unit->isChecked()) {
        config->set_hazard_unit(
            ui->hazard_stall->isChecked()
                ? machine::MachineConfig::HU_STALL
                : machine::MachineConfig::HU_STALL_FORWARD);
    } else {
        config->set_hazard_unit(machine::MachineConfig::HU_NONE);
    }
    switch2custom();
}

void NewDialog::mem_protec_exec_change(bool v) {
    config->set_memory_execute_protection(v);
    switch2custom();
}

void NewDialog::mem_protec_write_change(bool v) {
    config->set_memory_write_protection(v);
    switch2custom();
}

void NewDialog::mem_time_read_change(int v) {
    if (config->memory_access_time_read() != (unsigned)v) {
        config->set_memory_access_time_read(v);
        switch2custom();
    }
}

void NewDialog::mem_time_write_change(int v) {
    if (config->memory_access_time_write() != (unsigned)v) {
        config->set_memory_access_time_write(v);
        switch2custom();
    }
}

void NewDialog::mem_enable_burst_change(bool v) {
    if (config->memory_access_enable_burst() != v) {
        config->set_memory_access_enable_burst(v);
        switch2custom();
    }
}

void NewDialog::mem_time_burst_change(int v) {
    if (config->memory_access_time_burst() != (unsigned)v) {
        config->set_memory_access_time_burst(v);
        switch2custom();
    }
}

void NewDialog::mem_time_level2_change(int v) {
    if (config->memory_access_time_level2() != (unsigned)v) {
        config->set_memory_access_time_level2(v);
        switch2custom();
    }
}

void NewDialog::osemu_enable_change(bool v) {
    config->set_osemu_enable(v);
}

void NewDialog::osemu_known_syscall_stop_change(bool v) {
    config->set_osemu_known_syscall_stop(v);
}

void NewDialog::osemu_unknown_syscall_stop_change(bool v) {
    config->set_osemu_unknown_syscall_stop(v);
}

void NewDialog::osemu_interrupt_stop_change(bool v) {
    config->set_osemu_interrupt_stop(v);
}

void NewDialog::osemu_exception_stop_change(bool v) {
    config->set_osemu_exception_stop(v);
}

void NewDialog::browse_osemu_fs_root() {
    auto osemu_fs_root_dialog = new QFileDialog(this);
    osemu_fs_root_dialog->setFileMode(QFileDialog::Directory);
    osemu_fs_root_dialog->setOption(QFileDialog::ShowDirsOnly, true);
    QFileDialog::connect(osemu_fs_root_dialog, &QFileDialog::finished, [=](int result) {
        if (result > 0) {
            QString path = osemu_fs_root_dialog->selectedFiles()[0];
            ui->osemu_fs_root->setText(path);
            config->set_osemu_fs_root(path);
            delete osemu_fs_root_dialog;
        }
    });
    osemu_fs_root_dialog->open();
}

void NewDialog::osemu_fs_root_change(QString val) {
    config->set_osemu_fs_root(std::move(val));
}

void NewDialog::reset_at_compile_change(bool v) {
    config->set_reset_at_compile(v);
}

void NewDialog::config_gui() {
    // Basic
    ui->elf_file->setText(config->elf());
    ui->reset_at_compile->setChecked(config->reset_at_compile());
    // Core
    ui->xlen_64bit->setChecked(config->get_simulated_xlen() == machine::Xlen::_64);
    ui->isa_multiply->setChecked(config->get_isa_word().contains('M'));
    ui->isa_atomic->setChecked(config->get_isa_word().contains('A'));
    ui->pipelined->setChecked(config->pipelined());
    ui->delay_slot->setChecked(config->delay_slot());
    ui->hazard_unit->setChecked(config->hazard_unit() != machine::MachineConfig::HU_NONE);
    ui->hazard_stall->setChecked(config->hazard_unit() == machine::MachineConfig::HU_STALL);
    ui->hazard_stall_forward->setChecked(
        config->hazard_unit() == machine::MachineConfig::HU_STALL_FORWARD);
    // Memory
    ui->mem_protec_exec->setChecked(config->memory_execute_protection());
    ui->mem_protec_write->setChecked(config->memory_write_protection());
    ui->mem_time_read->setValue((int)config->memory_access_time_read());
    ui->mem_time_write->setValue((int)config->memory_access_time_write());
    ui->mem_time_burst->setValue((int)config->memory_access_time_burst());
    ui->mem_time_level2->setValue((int)config->memory_access_time_level2());
    ui->mem_enable_burst->setChecked((int)config->memory_access_enable_burst());
    // Cache
    cache_handler_d->config_gui();
    cache_handler_p->config_gui();
    cache_handler_l2->config_gui();
    // Operating system and exceptions
    ui->osemu_enable->setChecked(config->osemu_enable());
    ui->osemu_known_syscall_stop->setChecked(config->osemu_known_syscall_stop());
    ui->osemu_unknown_syscall_stop->setChecked(config->osemu_unknown_syscall_stop());
    ui->osemu_interrupt_stop->setChecked(config->osemu_interrupt_stop());
    ui->osemu_exception_stop->setChecked(config->osemu_exception_stop());
    ui->osemu_fs_root->setText(config->osemu_fs_root());

    // Disable various sections according to configuration
    ui->delay_slot->setEnabled(false);
    ui->hazard_unit->setEnabled(config->pipelined());
}

unsigned NewDialog::preset_number() {
    enum machine::ConfigPresets preset;
    if (ui->preset_no_pipeline->isChecked())
        preset = machine::CP_SINGLE;
    else if (ui->preset_no_pipeline_cache->isChecked())
        preset = machine::CP_SINGLE_CACHE;
    else if (ui->preset_pipelined_bare->isChecked())
        preset = machine::CP_PIPE_NO_HAZARD;
    else if (ui->preset_pipelined->isChecked())
        preset = machine::CP_PIPE;
    else
        return 0;
    return (unsigned)preset + 1;
}

unsigned NewDialog::load_code_from_number() {
    enum machine::ConfigLoadCodeFrom preset;
    if (ui->load_code_from_dwarf->isChecked())
        preset = machine::LoadCodeFromDwarf;
    else if (ui->load_code_from_local->isChecked())
        preset = machine::LoadCodeFromLocal;
    else
        return 0;
    return (unsigned)preset + 1;
}

void NewDialog::load_settings() {
    // Load config
    config.reset(new machine::MachineConfig(settings));
    cache_handler_d->set_config(config->access_cache_data());
    cache_handler_p->set_config(config->access_cache_program());
    cache_handler_l2->set_config(config->access_cache_level2());

    // Load preset
    unsigned preset = settings->value("Preset", 1).toUInt();
    if (preset != 0) {
        auto p = (enum machine::ConfigPresets)(preset - 1);
        config->preset(p);
        switch (p) {
        case machine::CP_SINGLE:
            ui->preset_no_pipeline->setChecked(true);
            break;
        case machine::CP_SINGLE_CACHE:
            ui->preset_no_pipeline_cache->setChecked(true);
            break;
        case machine::CP_PIPE_NO_HAZARD:
            ui->preset_pipelined_bare->setChecked(true);
            break;
        case machine::CP_PIPE: ui->preset_pipelined->setChecked(true); break;
        }
    } else {
        ui->preset_custom->setChecked(true);
    }

    config_gui();
}

void NewDialog::store_settings() {
    config->store(settings);

    // Presets are not stored in settings, so we have to store them explicitly
    if (ui->preset_custom->isChecked()) {
        settings->setValue("Preset", 0);
    } else {
        settings->setValue("Preset", preset_number());
    }
}

NewDialogCacheHandler::NewDialogCacheHandler(NewDialog *nd,
    Ui::NewDialogCache *cui) : Super(nd) {
    this->nd = nd;
    this->ui = cui;
    this->config = nullptr;
    connect(
        ui->enabled, &QGroupBox::clicked, this,
        &NewDialogCacheHandler::enabled);
    connect(
        ui->number_of_sets, &QAbstractSpinBox::editingFinished, this,
        &NewDialogCacheHandler::numsets);
    connect(
        ui->block_size, &QAbstractSpinBox::editingFinished, this,
        &NewDialogCacheHandler::blocksize);
    connect(
        ui->degree_of_associativity, &QAbstractSpinBox::editingFinished, this,
        &NewDialogCacheHandler::degreeassociativity);
    connect(
        ui->replacement_policy, QOverload<int>::of(&QComboBox::activated), this,
        &NewDialogCacheHandler::replacement);
    connect(
        ui->writeback_policy, QOverload<int>::of(&QComboBox::activated), this,
        &NewDialogCacheHandler::writeback);
}

void NewDialogCacheHandler::set_config(machine::CacheConfig *cache_config) {
    this->config = cache_config;
}

void NewDialogCacheHandler::config_gui() {
    ui->enabled->setChecked(config->enabled());
    ui->number_of_sets->setValue((int)config->set_count());
    ui->block_size->setValue((int)config->block_size());
    ui->degree_of_associativity->setValue((int)config->associativity());
    ui->replacement_policy->setCurrentIndex((int)config->replacement_policy());
    ui->writeback_policy->setCurrentIndex((int)config->write_policy());
}

void NewDialogCacheHandler::enabled(bool val) {
    config->set_enabled(val);
    nd->switch2custom();
}

void NewDialogCacheHandler::numsets() {
    config->set_set_count(ui->number_of_sets->value());
    nd->switch2custom();
}

void NewDialogCacheHandler::blocksize() {
    config->set_block_size(ui->block_size->value());
    nd->switch2custom();
}

void NewDialogCacheHandler::degreeassociativity() {
    config->set_associativity(ui->degree_of_associativity->value());
    nd->switch2custom();
}

void NewDialogCacheHandler::replacement(int val) {
    config->set_replacement_policy(
        (enum machine::CacheConfig::ReplacementPolicy)val);
    nd->switch2custom();
}

void NewDialogCacheHandler::writeback(int val) {
    config->set_write_policy((enum machine::CacheConfig::WritePolicy)val);
    nd->switch2custom();
}
