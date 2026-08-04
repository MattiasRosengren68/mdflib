/*
* Copyright 2025 Ingemar Hedvall
* SPDX-License-Identifier: MIT
 */
#include "testlinlogger.h"

#include <filesystem>

#include "util/logconfig.h"
#include "util/logstream.h"
#include "util/timestamp.h"

#include "mdf/linmessage.h"
#include "mdf/mdflogstream.h"
#include "mdf/iheader.h"
#include "mdf/ifilehistory.h"
#include "mdf/idatagroup.h"
#include "mdf/linconfigadapter.h"
#include "mdf/mdfreader.h"

using namespace util::log;
using namespace std::filesystem;
using namespace util::time;

namespace {

std::string kTestRootDir; ///< Test root directory (%TEMP%/test)
std::string kTestDir; ///<  'Test Root Dir'/mdf/flexray";
bool kSkipTest = false; ///< Set to true if the output dir is missing

/**
 * Function that connect the MDF library to the UTIL library log functionality.
 * @param location Source file and line location
 * @param severity Severity code
 * @param text Log text
 */
void LogFunc(const MdfLocation& location, mdf::MdfLogSeverity severity,
             const std::string& text) {
 const auto &log_config = LogConfig::Instance();
 LogMessage message;
 message.message = text;
 message.severity = static_cast<LogSeverity>(severity);

 log_config.AddLogMessage(message);
}
/**
 * Function that stops logging of MDF logs
 * @param location Source file and line location
 * @param severity Severity code
 * @param text Log text
 */
void NoLog(const MdfLocation& location , mdf::MdfLogSeverity severity,
           const std::string& text ) {
}

} // End namespace

namespace mdf::test {

void TestLinLogger::SetUpTestSuite() {

  try {
    // Create the root asn log directory. Note that this directory
    // exists in the temp dir of the operating system and is not
    // deleted by this test program. May be deleted at restart
    // of the operating system.
    auto temp_dir = temp_directory_path();
    temp_dir.append("test");
    kTestRootDir = temp_dir.string();
    create_directories(temp_dir); // Note never deleted

    // Log to the console instead of as normally to a file
    auto& log_config = LogConfig::Instance();
    log_config.Type(LogType::LogToConsole);
    log_config.CreateDefaultLogger();

    // Connect MDF library to use the util logging functionality
    MdfLogStream::SetLogFunction1(LogFunc);

    // Create the test directory. Note that this directory is deleted before
    // running the test, not after. This give the
    temp_dir.append("mdf");
    temp_dir.append("lin");

    // Remove older files here instead as normally in the tear down function.
    // This enables the end user to manually analyzing the generated files.
    std::error_code err;
    remove_all(temp_dir, err);
    if (err) {
      LOG_ERROR() << "Remove error. Message: " << err.message();
    }
    create_directories(temp_dir);
    kTestDir = temp_dir.string();

    LOG_INFO() << "Created the test directory. Dir: " << kTestDir;
    kSkipTest = false;

  } catch (const std::exception& err) {
    LOG_ERROR() << "Failed to create test directories. Error: " << err.what();
    kSkipTest = true;
  }
}

void TestLinLogger::TearDownTestSuite() {
  LOG_INFO() << "Tear down the test suite.";
  MdfLogStream::SetLogFunction1(NoLog);
  LogConfig& log_config = LogConfig::Instance();
  log_config.DeleteLogChain();
}

TEST_F(TestLinLogger, TestLinMessage) {
  auto writer = MdfFactory::CreateMdfWriter(MdfWriterType::MdfBusLogger);
  ASSERT_TRUE(writer);

  LinMessage msg;

  msg.BusChannel(5);
  EXPECT_EQ(msg.BusChannel(), 5);

  msg.LinId(0x3F);
  EXPECT_EQ(msg.LinId(), 0x3F);

  msg.Dir(true);
  EXPECT_TRUE(msg.Dir());

  msg.ReceivedDataByteCount(8);
  EXPECT_EQ(msg.ReceivedDataByteCount(), 8);

  msg.DataLength(7);
  EXPECT_EQ(msg.DataLength(), 7);

  std::vector<uint8_t> data = {0,1,2,3,4,5};
  msg.DataBytes(data);
  EXPECT_EQ(msg.DataBytes(), data);
  EXPECT_EQ(msg.DataLength(), 6);

  msg.Checksum(0xAA);
  EXPECT_EQ(msg.Checksum(),0xAA);

  msg.ChecksumModel(LinChecksumModel::Enhanced);
  EXPECT_EQ(msg.ChecksumModel(),LinChecksumModel::Enhanced);

  msg.StartOfFrame(1000);
  EXPECT_EQ(msg.StartOfFrame(),1000);

  msg.Baudrate(1200.0F);
  EXPECT_FLOAT_EQ(msg.Baudrate(),1200.0F);

  msg.ResponseBaudrate(2400.0F);
  EXPECT_FLOAT_EQ(msg.ResponseBaudrate(),2400.0F);

  msg.BreakLength(2000);
  EXPECT_EQ(msg.BreakLength(),2000);

  msg.DelimiterBreakLength(3000);
  EXPECT_EQ(msg.DelimiterBreakLength(),3000);

  msg.ExpectedDataByteCount(3);
  EXPECT_EQ(msg.ExpectedDataByteCount(),3);

  msg.TypeOfLongDominantSignal(LinTypeOfLongDominantSignal::CyclicReport);
  EXPECT_EQ(msg.TypeOfLongDominantSignal(),LinTypeOfLongDominantSignal::CyclicReport);

  msg.TotalSignalLength(1900);
  EXPECT_EQ(msg.TotalSignalLength(),1900);
}

TEST_F(TestLinLogger, Mdf4LinConfig) {
  if (kSkipTest) {
    GTEST_SKIP();
  }

  constexpr size_t max_samples = 100'000;
  path mdf_file(kTestDir);
  mdf_file.append("lin_mlsd_storage.mf4");

  auto writer = MdfFactory::CreateMdfWriter(MdfWriterType::MdfBusLogger);
  ASSERT_TRUE(writer);
  writer->Init(mdf_file.string());
  writer->BusType(MdfBusType::LIN);
  writer->StorageType(MdfStorageType::MlsdStorage);
  writer->MaxLength(8);
  writer->PreTrigTime(0.0);
  writer->CompressData(false);

  auto* header = writer->Header();
  ASSERT_TRUE(header != nullptr);

  auto* history = header->CreateFileHistory();
  ASSERT_TRUE(history != nullptr);
  FhComment fh_comment;
  fh_comment.Comment("Test data types");
  fh_comment.ToolId("MdfWrite");
  fh_comment.ToolVendor("ACME Road Runner Company");
  fh_comment.ToolVersion("1.0");
  fh_comment.UserName("Ingemar Hedvall");
  history->SetFhComment(fh_comment);

; IDataGroup* last_dg = header->CreateDataGroup();
  ASSERT_TRUE(last_dg != nullptr);

  LinConfigAdapter config(*writer);
  config.CreateConfig(*last_dg);

  IChannelGroup* lin_frame = last_dg->GetChannelGroup("LIN_Frame");
  ASSERT_TRUE(lin_frame != nullptr);

  IChannelGroup* lin_wake_up = last_dg->GetChannelGroup("LIN_WakeUp");
  ASSERT_TRUE(lin_wake_up != nullptr);

  IChannelGroup* lin_checksum_error = last_dg->GetChannelGroup("LIN_ChecksumError");
  ASSERT_TRUE(lin_checksum_error != nullptr);

  IChannelGroup* lin_transmission_error = last_dg->GetChannelGroup("LIN_TransmissionError");
  ASSERT_TRUE(lin_transmission_error != nullptr);

  IChannelGroup* lin_sync_error = last_dg->GetChannelGroup("LIN_SyncError");
  ASSERT_TRUE(lin_sync_error != nullptr);

  IChannelGroup* lin_receive_error = last_dg->GetChannelGroup("LIN_ReceiveError");
  ASSERT_TRUE(lin_receive_error != nullptr);

  IChannelGroup* lin_spike = last_dg->GetChannelGroup("LIN_Spike");
  ASSERT_TRUE(lin_spike != nullptr);

  IChannelGroup* lin_long_dom = last_dg->GetChannelGroup("LIN_LongDom");
  ASSERT_TRUE(lin_long_dom != nullptr);

  writer->InitMeasurement();
  auto tick_time = TimeStampToNs();
  writer->StartMeasurement(tick_time);
  for (size_t sample = 0; sample < max_samples; ++sample) {

    std::vector<uint8_t> data;
    data.assign(sample < 8 ? sample + 1 : 8, static_cast<uint8_t>(sample + 1));

    LinMessage msg;
    msg.LinId(12);
    msg.BusChannel(11);
    EXPECT_EQ(msg.BusChannel(), 11);
    msg.DataBytes(data);

    // Add dummy message to all for message types. Not realistic
    // but makes the test simpler.
    writer->SaveLinMessage(*lin_frame, tick_time, msg);
    writer->SaveLinMessage(*lin_wake_up, tick_time, msg);
    writer->SaveLinMessage(*lin_checksum_error, tick_time, msg);
    writer->SaveLinMessage(*lin_transmission_error, tick_time, msg);
    writer->SaveLinMessage(*lin_sync_error, tick_time, msg);
    writer->SaveLinMessage(*lin_receive_error, tick_time, msg);
    writer->SaveLinMessage(*lin_spike, tick_time, msg);
    writer->SaveLinMessage(*lin_long_dom, tick_time, msg);

    tick_time += 1'000'000;
  }
  writer->StopMeasurement(tick_time);
  writer->FinalizeMeasurement();

  MdfReader reader(mdf_file.string());
  ChannelObserverList observer_list;

  ASSERT_TRUE(reader.IsOk());
  ASSERT_TRUE(reader.ReadEverythingButData());
  const auto* file1 = reader.GetFile();
  ASSERT_TRUE(file1 != nullptr);

  const auto* header1 = file1->Header();
  ASSERT_TRUE(header1 != nullptr);

  const auto dg_list = header1->DataGroups();
  EXPECT_EQ(dg_list.size(), 1);

  for (auto* dg4 : dg_list) {
    const auto cg_list = dg4->ChannelGroups();
    EXPECT_EQ(cg_list.size(), 8);
    CreateChannelObserverForDataGroup(*dg4, observer_list);
    reader.ReadData(*dg4);
  }
  reader.Close();

  std::set<std::string> unique_list;

  for (auto& observer : observer_list) {
    ASSERT_TRUE(observer);
    EXPECT_EQ(observer->NofSamples(), max_samples);

    // Check that all values are valid.
    const auto& valid_list = observer->GetValidList();
    const bool all_valid =  std::ranges::all_of(valid_list,
      [] (const bool& valid) -> bool {
      return valid;
    });
    EXPECT_TRUE(all_valid) << observer->Name();

    const auto& name = observer->Name();
    if (!unique_list.contains(name)) {
      unique_list.emplace(name);
    } else if (name != "t")  {
      EXPECT_TRUE(false) << "Duplicate: " << name;
    }
  }
}

TEST_F(TestLinLogger, Mdf4LinMandatory) {
  if (kSkipTest) {
    GTEST_SKIP();
  }
  path mdf_file(kTestDir);
  mdf_file.append("lin_mandatory.mf4");

  auto writer = MdfFactory::CreateMdfWriter(MdfWriterType::MdfBusLogger);
  writer->Init(mdf_file.string());
  writer->BusType(MdfBusType::LIN);
  writer->StorageType(MdfStorageType::MlsdStorage);
  writer->MaxLength(8);
  writer->MandatoryMembersOnly(true);
  writer->PreTrigTime(0.0);
  writer->CompressData(false);

  auto* header = writer->Header();
  auto* history = header->CreateFileHistory();
  history->Description("Test data types");
  history->ToolName("MdfWrite");
  history->ToolVendor("ACME Road Runner Company");
  history->ToolVersion("1.0");
  history->UserName("Ingemar Hedvall");

  IDataGroup* last_dg = header->CreateDataGroup();
  ASSERT_TRUE(last_dg != nullptr);

  EXPECT_TRUE(writer->CreateBusLogConfiguration());

  IChannelGroup* lin_frame = last_dg->GetChannelGroup("LIN_Frame");
  ASSERT_TRUE(lin_frame != nullptr);

  IChannelGroup* lin_wake_up = last_dg->GetChannelGroup("LIN_WakeUp");
  ASSERT_TRUE(lin_wake_up != nullptr);

  IChannelGroup* lin_checksum_error = last_dg->GetChannelGroup("LIN_ChecksumError");
  ASSERT_TRUE(lin_checksum_error != nullptr);

  IChannelGroup* lin_transmission_error = last_dg->GetChannelGroup("LIN_TransmissionError");
  ASSERT_TRUE(lin_transmission_error != nullptr);

  IChannelGroup* lin_sync_error = last_dg->GetChannelGroup("LIN_SyncError");
  ASSERT_TRUE(lin_sync_error != nullptr);

  IChannelGroup* lin_receive_error = last_dg->GetChannelGroup("LIN_ReceiveError");
  ASSERT_TRUE(lin_receive_error != nullptr);

  IChannelGroup* lin_spike = last_dg->GetChannelGroup("LIN_Spike");
  ASSERT_TRUE(lin_spike != nullptr);

  IChannelGroup* lin_long_dom = last_dg->GetChannelGroup("LIN_LongDom");
  ASSERT_TRUE(lin_long_dom != nullptr);

  writer->InitMeasurement();
  auto tick_time = TimeStampToNs();
  writer->StartMeasurement(tick_time);
  for (size_t sample = 0; sample < 100'000; ++sample) {

    std::vector<uint8_t> data;
    data.assign(sample < 8 ? sample + 1 : 8, static_cast<uint8_t>(sample + 1));

    LinMessage msg;
    msg.LinId(12);
    msg.BusChannel(11);
    EXPECT_EQ(msg.BusChannel(), 11);
    msg.DataBytes(data);

    // Add dummy message to all for message types. Not realistic
    // but makes the test simpler.
    writer->SaveLinMessage(*lin_frame, tick_time, msg);
    writer->SaveLinMessage(*lin_wake_up, tick_time, msg);
    writer->SaveLinMessage(*lin_checksum_error, tick_time, msg);
    writer->SaveLinMessage(*lin_transmission_error, tick_time, msg);
    writer->SaveLinMessage(*lin_sync_error, tick_time, msg);
    writer->SaveLinMessage(*lin_receive_error, tick_time, msg);
    writer->SaveLinMessage(*lin_spike, tick_time, msg);
    writer->SaveLinMessage(*lin_long_dom, tick_time, msg);

    tick_time += 1'000'000;
  }
  writer->StopMeasurement(tick_time);
  writer->FinalizeMeasurement();

  MdfReader reader(mdf_file.string());
  ChannelObserverList observer_list;

  ASSERT_TRUE(reader.IsOk());
  ASSERT_TRUE(reader.ReadEverythingButData());
  const auto* file1 = reader.GetFile();
  const auto* header1 = file1->Header();
  const auto dg_list = header1->DataGroups();
  EXPECT_EQ(dg_list.size(), 1);

  for (auto* dg4 : dg_list) {
    const auto cg_list = dg4->ChannelGroups();
    EXPECT_EQ(cg_list.size(), 8);
    CreateChannelObserverForDataGroup(*dg4, observer_list);
    reader.ReadData(*dg4);
  }
  reader.Close();

  std::set<std::string> unique_list;

  for (auto& observer : observer_list) {
    ASSERT_TRUE(observer);
    EXPECT_EQ(observer->NofSamples(), 100'000);
    const auto& valid_list = observer->GetValidList();
    const bool all_valid =  std::ranges::all_of( valid_list,
           [] (const bool& valid) -> bool {
      return valid;
    });

    EXPECT_TRUE(all_valid) << observer->Name();

    // Verify that the CAN_RemoteFrame.DLC exist
    const auto name = observer->Name();
    if (!unique_list.contains(name)) {
      unique_list.emplace(name);
    } else if (name != "t")  {
      EXPECT_TRUE(false) << "Duplicate: " << name;
    }
  }
}

TEST_F(TestLinLogger, TestCustomConfig) {
  if (kSkipTest) {
    GTEST_SKIP();
  }

  constexpr size_t max_sample = 10;
  const std::vector<uint8_t> sample_data({0xAA, 0xBB});
  path mdf_file(kTestDir);
  mdf_file.append("lin_custom_sorted.mf4");
  {
    auto writer = MdfFactory::CreateMdfWriter(MdfWriterType::MdfBusLogger);
    ASSERT_TRUE(writer);
    writer->Init(mdf_file.string());

    auto* header = writer->Header();
    ASSERT_TRUE(header != nullptr);
    HdComment hd_comment("Test of LIN Custom Config");
    hd_comment.Author("Ingemar Hedvall");
    hd_comment.Department("IH Development");
    hd_comment.Project("LIN bus support");
    hd_comment.Subject("Testing LIN support");
    hd_comment.TimeSource(MdString("PC UTC Time"));
    header->SetHdComment(hd_comment);

    auto* history = header->CreateFileHistory();
    ASSERT_TRUE(history != nullptr);
    FhComment fh_comment("Initial file change");
    fh_comment.ToolId("Google Unit Test");
    fh_comment.ToolVendor("ACME Road Runner Company");
    fh_comment.ToolVersion("1.0");
    fh_comment.UserName("ihedvall");
    history->SetFhComment(fh_comment);

    writer->BusType(MdfBusType::LIN);
    writer->StorageType(MdfStorageType::MlsdStorage);
    writer->PreTrigTime(0.0);
    writer->CompressData(false);
    writer->MandatoryMembersOnly(false);

    LinConfigAdapter lin_config(*writer);
     // Create a DG block
    IDataGroup* dg_data_frame = header->CreateDataGroup();
    ASSERT_TRUE(dg_data_frame != nullptr);

     IChannelGroup* lin_frame = lin_config.CreateCustomConfig(
      *dg_data_frame, LinMessageType::LIN_Frame);
    ASSERT_TRUE(lin_frame != nullptr);

    IDataGroup* dg_error_frame = header->CreateDataGroup();
    ASSERT_TRUE(dg_error_frame != nullptr);
    IChannelGroup* error_frame = lin_config.CreateCustomConfig(
      *dg_error_frame, LinMessageType::LIN_ReceiveError);
    ASSERT_TRUE(error_frame != nullptr);

    writer->InitMeasurement();
    uint64_t tick_time = TimeStampToNs();
    writer->StartMeasurement(tick_time);

    for (size_t sample = 0; sample < max_sample; ++sample) {

      LinMessage d_frame(LinMessageType::LIN_Frame);
      d_frame.DataBytes(sample_data);

      LinMessage e_frame(LinMessageType::LIN_ReceiveError);
      e_frame.DataBytes(sample_data);

      writer->SaveLinMessage(*dg_data_frame, *lin_frame, tick_time, d_frame);
      tick_time += 1'000'000;
      writer->SaveLinMessage(*dg_error_frame, *error_frame, tick_time, e_frame);
      tick_time += 1'000'000;
    }

    writer->StopMeasurement(tick_time);
    writer->FinalizeMeasurement();
  }

  {
    MdfReader reader(mdf_file.string());
    ChannelObserverList observer_list;

    ASSERT_TRUE(reader.IsOk());
    ASSERT_TRUE(reader.ReadEverythingButData());
    const MdfFile* file = reader.GetFile();
    ASSERT_TRUE(file != nullptr);

    const IHeader* header = file->Header();
    ASSERT_TRUE(header != nullptr);

    const auto dg_list = header->DataGroups();
    EXPECT_EQ(dg_list.size(), 2);

    for (IDataGroup* data_group : dg_list) {
      ASSERT_TRUE(data_group != nullptr);

      const auto cg_list = data_group->ChannelGroups();
      EXPECT_EQ(cg_list.size(), 1);
      for (IChannelGroup* channel_group : cg_list) {
        ASSERT_TRUE(channel_group != nullptr);
        CreateChannelObserverForChannelGroup(*data_group, *channel_group,
                                             observer_list);
      }
      reader.ReadData(*data_group);
      bool data_ok = false;
      for (auto& observer : observer_list) {
        ASSERT_TRUE(observer);
        const uint64_t nof_samples = observer->NofSamples();

        EXPECT_EQ(nof_samples, max_sample);
        const auto& channel = observer->Channel();
        if (channel.DataType() == ChannelDataType::ByteArray &&
            (channel.Type() == ChannelType::VariableLength ||
             channel.Type() == ChannelType::MaxLength) ) {
          for (uint64_t sample = 0; sample < max_sample; ++sample) {
            std::vector<uint8_t> value_list;
            const bool valid = observer->GetEngValue(sample, value_list);
            EXPECT_TRUE(valid) << "Sample: " << sample;
            EXPECT_EQ(value_list, sample_data);
            data_ok = true;
          }
        }


      }
      EXPECT_TRUE(data_ok);
    }
    reader.Close();
  }

}

} // mdf