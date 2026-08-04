/*
* Copyright 2026 Ingemar Hedvall
* SPDX-License-Identifier: MIT
*/

#include "testsortingdata.h"

#include <array>
#include <filesystem>

#include "mdf/canconfigadapter.h"
#include "mdf/linconfigadapter.h"
#include "mdf/flexrayconfigadapter.h"

#include "mdf/fhcomment.h"
#include "mdf/mdflogstream.h"
#include "mdf/ifilehistory.h"
#include "mdf/idatagroup.h"
#include "mdf/flexraymessage.h"

using namespace std::filesystem;

namespace {
std::string kTestRootDir; ///< Test root directory
std::string kTestDir; ///<  <Test Root Dir>/mdf/data";
bool kSkipTest = false;

constexpr std::array<mdf::test::TestDataParameter, 6> kTestInputList = {
  mdf::test::TestDataParameter{false, false,
    mdf::MdfStorageType::FixedLengthStorage},
  mdf::test::TestDataParameter{false, false,
    mdf::MdfStorageType::VlsdStorage},
  mdf::test::TestDataParameter{false, false,
    mdf::MdfStorageType::MlsdStorage},
  mdf::test::TestDataParameter{true, false,
  mdf::MdfStorageType::FixedLengthStorage},
  mdf::test::TestDataParameter{true, false,
    mdf::MdfStorageType::VlsdStorage},
  mdf::test::TestDataParameter{true, false,
    mdf::MdfStorageType::MlsdStorage},
};

};

namespace mdf::test {
void TestSortingData::SetUpTestSuite() {

  try {
    // Create the root asn log directory. Note that this directory
    // exists in the temp dir of the operating system and is not
    // deleted by this test program. May be deleted at restart
    // of the operating system.
    auto temp_dir = temp_directory_path();
    temp_dir.append("test");
    kTestRootDir = temp_dir.string();
    create_directories(temp_dir); // Not deleted


    // Log to the console instead of as normally to a file
    MdfLogStream::SetLogFunction1(MdfLogStream::LogToConsole);

    // Create the test directory. Note that this directory is deleted before
    // running the test, not after. This give the
    temp_dir.append("mdf");
    temp_dir.append("data");
    std::error_code err;
    remove_all(temp_dir, err);
    if (err) {
      MDF_TRACE() << "Remove error. Message: " << err.message();
    }
    create_directories(temp_dir);
    kTestDir = temp_dir.string();

    MDF_TRACE() << "Created the test directory. Dir: " << kTestDir;
    kSkipTest = false;

  } catch (const std::exception& err) {
    MDF_ERROR() << "Failed to create test directories. Error: " << err.what();
    kSkipTest = true;
  }
}

void TestSortingData::TearDownTestSuite() {
  MDF_TRACE() << "Tear down the test suite.";
  MdfLogStream::ResetLogFunction();
}

void TestSortingData::TestDataFile(const std::wstring& filename,
  bool sorted,
  size_t max_sample,
  const std::vector<uint8_t>& sample_data) {

  const path mdf_file(filename);
  const path test_name = mdf_file.stem();

  MdfReader reader(filename);
  ChannelObserverList observer_list;

  ASSERT_TRUE(reader.IsOk());
  ASSERT_TRUE(reader.ReadEverythingButData());
  const MdfFile* file = reader.GetFile();
  ASSERT_TRUE(file != nullptr);

  const IHeader* header = file->Header();
  ASSERT_TRUE(header != nullptr);

  const auto dg_list = header->DataGroups();
  if (!sorted) {
    EXPECT_EQ(dg_list.size(), 1);
  } else {
    EXPECT_GT(dg_list.size(), 1);
  }

  for (IDataGroup* data_group : dg_list) {
    ASSERT_TRUE(data_group != nullptr);

    const auto cg_list = data_group->ChannelGroups();
    if (sorted) {
      EXPECT_EQ(cg_list.size(), 1);
    } else {
      EXPECT_GE(cg_list.size(), 3);
    }
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

      EXPECT_EQ(nof_samples, max_sample) << test_name;
      const auto& channel = observer->Channel();
      if (channel.DataType() == ChannelDataType::ByteArray &&
          ( channel.Type() == ChannelType::VariableLength ||
            channel.Type() == ChannelType::MaxLength) ) {
        for (uint64_t sample = 0; sample < max_sample; ++sample) {
          std::vector<uint8_t> value_list;
          const bool valid = observer->GetEngValue(sample, value_list);
          EXPECT_TRUE(valid) << "Sample: " << sample << test_name;
          EXPECT_EQ(value_list, sample_data) << "Sample: " << sample << test_name;

          if (!valid) {
            break;
          }
          data_ok = true;
        }
      }
    }
    EXPECT_TRUE(data_ok);
  }
  reader.Close();

}

TEST_P(TestSortingData, TestCanData) {
  if (kSkipTest) {
    GTEST_SKIP();
  }
  TestDataParameter test_input = GetParam();
  constexpr size_t max_sample = 10;
  const std::vector<uint8_t> sample_data({0xAA, 0xBB});

  path mdf_file(kTestDir);
  mdf_file.append("can_data");
  if (test_input.mandatory_only) {
    mdf_file += ("_man");
  }
  if (test_input.compress_data) {
    mdf_file += ("_com");
  }
  if (test_input.storage_type == MdfStorageType::VlsdStorage) {
    mdf_file += ("_vlsd");
  } else if (test_input.storage_type == MdfStorageType::MlsdStorage) {
    mdf_file += ("_mlsd");
  }

  mdf_file += ".mf4";

 {
    auto writer = MdfFactory::CreateMdfWriter(MdfWriterType::MdfBusLogger);
    ASSERT_TRUE(writer);
    writer->Init(mdf_file.string());

    auto* header = writer->Header();
    ASSERT_TRUE(header != nullptr);
    HdComment hd_comment("Test of CAN Data");
    hd_comment.Author("Ingemar Hedvall");
    hd_comment.Department("IH Development");
    hd_comment.Project("CAN bus support");
    hd_comment.Subject("Testing CAN support");
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

    writer->BusType(MdfBusType::CAN);
    writer->StorageType(test_input.storage_type);
    writer->PreTrigTime(0.0);
    writer->CompressData(test_input.compress_data);
    writer->MandatoryMembersOnly(test_input.mandatory_only);

    // Create a DG block
    auto* last_dg = header->CreateDataGroup();
    ASSERT_TRUE(last_dg != nullptr);

    CanConfigAdapter can_config(*writer);
    can_config.CreateConfig(*last_dg);

    IChannelGroup* can_frame = last_dg->GetChannelGroup("CAN_DataFrame");
    ASSERT_TRUE(can_frame != nullptr);

    IChannelGroup* can_remote = last_dg->GetChannelGroup("CAN_RemoteFrame");
    ASSERT_TRUE(can_remote != nullptr);

    IChannelGroup* can_error = last_dg->GetChannelGroup("CAN_ErrorFrame");
    ASSERT_TRUE(can_error!= nullptr);

    IChannelGroup* can_overload = last_dg->GetChannelGroup("CAN_OverloadFrame");
    // Not included if mandatory only

    writer->InitMeasurement();
    uint64_t tick_time = MdfHelper::NowNs();
    writer->StartMeasurement(tick_time);

    for (size_t sample = 0; sample < max_sample; ++sample) {

      CanMessage d_frame(MessageType::CAN_DataFrame);
      d_frame.DataBytes(sample_data);
      writer->SaveCanMessage(*last_dg, *can_frame, tick_time, d_frame);
      tick_time += 1'000'000;

      CanMessage r_frame(MessageType::CAN_RemoteFrame);
      r_frame.DataBytes(sample_data);
      writer->SaveCanMessage(*last_dg, *can_remote, tick_time, r_frame);
      tick_time += 1'000'000;

      CanMessage e_frame(MessageType::CAN_ErrorFrame);
      e_frame.DataBytes(sample_data);
      writer->SaveCanMessage(*last_dg, *can_error, tick_time, e_frame);
      tick_time += 1'000'000;


      if (can_overload != nullptr) {
        CanMessage o_frame(MessageType::CAN_OverloadFrame);
        o_frame.DataBytes(sample_data);
        writer->SaveCanMessage(*last_dg, *can_overload, tick_time, o_frame);
        tick_time += 1'000'000;
      }
    }

    writer->StopMeasurement(tick_time);
    writer->FinalizeMeasurement();
  }

  TestDataFile(mdf_file.wstring(), false, max_sample, sample_data);

  path sorted_file = mdf_file.parent_path();
  sorted_file /= mdf_file.stem();
  sorted_file += "_sorted";
  sorted_file += mdf_file.extension();
  {
    auto sortTask =
            mdf::MdfFactory::CreateTask(mdf::MdfTaskType::MdfSortingTask);

    sortTask->SourceFile(mdf_file.string());

    sortTask->DestinationFile(sorted_file.string());
    sortTask->SkipIfNoSamples(true);
    sortTask->Run();
  }
  TestDataFile(sorted_file.wstring(), true, max_sample, sample_data);

}

TEST_P(TestSortingData, TestLinData) {
  if (kSkipTest) {
    GTEST_SKIP();
  }
  TestDataParameter test_input = GetParam();
  constexpr size_t max_sample = 10;
  const std::vector<uint8_t> sample_data({0xAA, 0xBB});

  path mdf_file(kTestDir);
  mdf_file.append("lin_data");
  if (test_input.mandatory_only) {
    mdf_file += ("_man");
  }
  if (test_input.compress_data) {
    mdf_file += ("_com");
  }
  if (test_input.storage_type == MdfStorageType::VlsdStorage) {
    mdf_file += ("_vlsd");
  } else if (test_input.storage_type == MdfStorageType::MlsdStorage) {
    mdf_file += ("_mlsd");
  }

  mdf_file += ".mf4";

 {
    auto writer = MdfFactory::CreateMdfWriter(MdfWriterType::MdfBusLogger);
    ASSERT_TRUE(writer);
    writer->Init(mdf_file.string());

    auto* header = writer->Header();
    ASSERT_TRUE(header != nullptr);
    HdComment hd_comment("Test of LIN Data");
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
    writer->StorageType(test_input.storage_type);
    writer->PreTrigTime(0.0);
    writer->CompressData(test_input.compress_data);
    writer->MandatoryMembersOnly(test_input.mandatory_only);

    // Create a DG block
    auto* last_dg = header->CreateDataGroup();
    ASSERT_TRUE(last_dg != nullptr);

    LinConfigAdapter lin_config(*writer);
    lin_config.CreateConfig(*last_dg);

    IChannelGroup* lin_frame = last_dg->GetChannelGroup("LIN_Frame");
    ASSERT_TRUE(lin_frame != nullptr);

    IChannelGroup* lin_checksum_error = last_dg->GetChannelGroup("LIN_ChecksumError");
    ASSERT_TRUE(lin_checksum_error != nullptr);

    IChannelGroup* lin_receive_error = last_dg->GetChannelGroup("LIN_ReceiveError");
    ASSERT_TRUE(lin_receive_error!= nullptr);

    IChannelGroup* lin_sync_error = last_dg->GetChannelGroup("LIN_SyncError");
    ASSERT_TRUE(lin_sync_error!= nullptr);

    IChannelGroup* lin_transmission_error = last_dg->GetChannelGroup("LIN_TransmissionError");
    ASSERT_TRUE(lin_transmission_error!= nullptr);

    IChannelGroup* lin_wake_up = last_dg->GetChannelGroup("LIN_WakeUp");
    ASSERT_TRUE(lin_wake_up!= nullptr);

    IChannelGroup* lin_spike = last_dg->GetChannelGroup("LIN_Spike");
    ASSERT_TRUE(lin_spike != nullptr);

    IChannelGroup* lin_long_dom = last_dg->GetChannelGroup("LIN_LongDom");
    ASSERT_TRUE(lin_long_dom != nullptr);

    writer->InitMeasurement();
    uint64_t tick_time = MdfHelper::NowNs();
    writer->StartMeasurement(tick_time);

    for (size_t sample = 0; sample < max_sample; ++sample) {

      LinMessage data_frame;
      data_frame.DataBytes(sample_data);
      writer->SaveLinMessage(*last_dg, *lin_frame, tick_time,data_frame);
      tick_time += 1'000'000;

      writer->SaveLinMessage(*last_dg, *lin_checksum_error, tick_time, data_frame);
      tick_time += 1'000'000;

      writer->SaveLinMessage(*last_dg, *lin_receive_error, tick_time, data_frame);
      tick_time += 1'000'000;

      writer->SaveLinMessage(*last_dg, *lin_sync_error, tick_time, data_frame);
      tick_time += 1'000'000;

      writer->SaveLinMessage(*last_dg, *lin_transmission_error, tick_time,data_frame);
      tick_time += 1'000'000;

      writer->SaveLinMessage(*last_dg, *lin_wake_up, tick_time, data_frame);
      tick_time += 1'000'000;

      writer->SaveLinMessage(*last_dg, *lin_spike, tick_time, data_frame);
      tick_time += 1'000'000;

      writer->SaveLinMessage(*last_dg, *lin_long_dom, tick_time, data_frame);
      tick_time += 1'000'000;
    }

    writer->StopMeasurement(tick_time);
    writer->FinalizeMeasurement();
  }

  TestDataFile(mdf_file.wstring(), false, max_sample, sample_data);

  path sorted_file = mdf_file.parent_path();
  sorted_file /= mdf_file.stem();
  sorted_file += "_sorted";
  sorted_file += mdf_file.extension();
  {
    auto sortTask =
            mdf::MdfFactory::CreateTask(mdf::MdfTaskType::MdfSortingTask);

    sortTask->SourceFile(mdf_file.string());

    sortTask->DestinationFile(sorted_file.string());
    sortTask->SkipIfNoSamples(true);
    sortTask->Run();
  }
  TestDataFile(sorted_file.wstring(), true, max_sample, sample_data);

}

TEST_P(TestSortingData, TestFlexRayData) {
  if (kSkipTest) {
    GTEST_SKIP();
  }
  TestDataParameter test_input = GetParam();
  constexpr size_t max_sample = 10;
  const std::vector<uint8_t> sample_data({0xAA, 0xBB});

  path mdf_file(kTestDir);
  mdf_file.append("flexray_data");
  if (test_input.mandatory_only) {
    mdf_file += ("_man");
  }
  if (test_input.compress_data) {
    mdf_file += ("_com");
  }
  if (test_input.storage_type == MdfStorageType::VlsdStorage) {
    mdf_file += ("_vlsd");
  } else if (test_input.storage_type == MdfStorageType::MlsdStorage) {
    mdf_file += ("_mlsd");
  }

  mdf_file += ".mf4";

 {
    auto writer = MdfFactory::CreateMdfWriter(MdfWriterType::MdfBusLogger);
    ASSERT_TRUE(writer);
    writer->Init(mdf_file.string());

    auto* header = writer->Header();
    ASSERT_TRUE(header != nullptr);
    HdComment hd_comment("Test of FlexRay Data");
    hd_comment.Author("Ingemar Hedvall");
    hd_comment.Department("IH Development");
    hd_comment.Project("FlexRay bus support");
    hd_comment.Subject("Testing FlexRay support");
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

    writer->BusType(MdfBusType::FlexRay);
    writer->StorageType(test_input.storage_type);
    if (writer->StorageType() == MdfStorageType::MlsdStorage) {
      writer->StorageType(MdfStorageType::VlsdStorage);
    }
    writer->PreTrigTime(0.0);
    writer->CompressData(test_input.compress_data);
    writer->MandatoryMembersOnly(test_input.mandatory_only);

    // Create a DG block
    auto* last_dg = header->CreateDataGroup();
    ASSERT_TRUE(last_dg != nullptr);

    FlexRayConfigAdapter flexray_config(*writer);
    flexray_config.CreateConfig(*last_dg);

    IChannelGroup* flx_frame = last_dg->GetChannelGroup("FLX_Frame");
    ASSERT_TRUE(flx_frame != nullptr);

    IChannelGroup* flx_pdu = last_dg->GetChannelGroup("FLX_Pdu");
    ASSERT_TRUE(flx_pdu != nullptr);

    IChannelGroup* flx_frame_header = last_dg->GetChannelGroup("FLX_FrameHeader");
    ASSERT_TRUE(flx_frame_header != nullptr);

    IChannelGroup* flx_null_frame = last_dg->GetChannelGroup("FLX_NullFrame");
    ASSERT_TRUE(flx_null_frame != nullptr);

    IChannelGroup* flx_error_frame = last_dg->GetChannelGroup("FLX_ErrorFrame");
    ASSERT_TRUE(flx_error_frame != nullptr);

    IChannelGroup* flx_symbol = last_dg->GetChannelGroup("FLX_Symbol");
    ASSERT_TRUE(flx_symbol != nullptr);

    writer->InitMeasurement();
    uint64_t tick_time = MdfHelper::NowNs();
    writer->StartMeasurement(tick_time);

    for (size_t sample = 0; sample < max_sample; ++sample) {

      FlexRayFrame d_frame;
      d_frame.DataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_frame, tick_time, d_frame);
      tick_time += 1'000'000;

      FlexRayErrorFrame e_frame;
      e_frame.DataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_error_frame, tick_time, e_frame);
      tick_time += 1'000'000;

      FlexRayPdu p_frame;
      p_frame.DataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_pdu, tick_time, p_frame);
      tick_time += 1'000'000;

      FlexRayFrameHeader f_frame;
      f_frame.FillDataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_frame_header, tick_time, f_frame);
      tick_time += 1'000'000;

      FlexRayNullFrame n_frame;
      n_frame.DataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_null_frame, tick_time, n_frame);
      tick_time += 1'000'000;

      FlexRaySymbol s_frame;
      writer->SaveFlexRayMessage(*last_dg, *flx_symbol, tick_time, s_frame);
      tick_time += 1'000'000;

    }

    writer->StopMeasurement(tick_time);
    writer->FinalizeMeasurement();
  }

  TestDataFile(mdf_file.wstring(), false, max_sample, sample_data);

  path sorted_file = mdf_file.parent_path();
  sorted_file /= mdf_file.stem();
  sorted_file += "_sorted";
  sorted_file += mdf_file.extension();
  {
    auto sortTask =
            mdf::MdfFactory::CreateTask(mdf::MdfTaskType::MdfSortingTask);

    sortTask->SourceFile(mdf_file.string());

    sortTask->DestinationFile(sorted_file.string());
    sortTask->SkipIfNoSamples(true);
    sortTask->Run();
  }
  TestDataFile(sorted_file.wstring(), true, max_sample, sample_data);

}
TEST_P(TestSortingData, TestMostData) {
  if (kSkipTest) {
    GTEST_SKIP();
  }
  TestDataParameter test_input = GetParam();
  constexpr size_t max_sample = 10;
  const std::vector<uint8_t> sample_data({0xAA, 0xBB});

  path mdf_file(kTestDir);
  mdf_file.append("most_data");
  if (test_input.mandatory_only) {
    mdf_file += ("_man");
  }
  if (test_input.compress_data) {
    mdf_file += ("_com");
  }
  if (test_input.storage_type == MdfStorageType::VlsdStorage) {
    mdf_file += ("_vlsd");
  } else if (test_input.storage_type == MdfStorageType::MlsdStorage) {
    mdf_file += ("_mlsd");
  }

  mdf_file += ".mf4";

 {
    auto writer = MdfFactory::CreateMdfWriter(MdfWriterType::MdfBusLogger);
    ASSERT_TRUE(writer);
    writer->Init(mdf_file.string());

    auto* header = writer->Header();
    ASSERT_TRUE(header != nullptr);
    HdComment hd_comment("Test of MOST Data");
    hd_comment.Author("Ingemar Hedvall");
    hd_comment.Department("IH Development");
    hd_comment.Project("MOST bus support");
    hd_comment.Subject("Testing MOST support");
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

    writer->BusType(MdfBusType::MOST);
    writer->StorageType(test_input.storage_type);
    if (writer->StorageType() == MdfStorageType::MlsdStorage) {
      writer->StorageType(MdfStorageType::VlsdStorage);
    }
    writer->PreTrigTime(0.0);
    writer->CompressData(test_input.compress_data);
    writer->MandatoryMembersOnly(test_input.mandatory_only);

    // Create a DG block
    auto* last_dg = header->CreateDataGroup();
    ASSERT_TRUE(last_dg != nullptr);

    FlexRayConfigAdapter flexray_config(*writer);
    flexray_config.CreateConfig(*last_dg);

    IChannelGroup* flx_frame = last_dg->GetChannelGroup("FLX_Frame");
    ASSERT_TRUE(flx_frame != nullptr);

    IChannelGroup* flx_pdu = last_dg->GetChannelGroup("FLX_Pdu");
    ASSERT_TRUE(flx_pdu != nullptr);

    IChannelGroup* flx_frame_header = last_dg->GetChannelGroup("FLX_FrameHeader");
    ASSERT_TRUE(flx_frame_header != nullptr);

    IChannelGroup* flx_null_frame = last_dg->GetChannelGroup("FLX_NullFrame");
    ASSERT_TRUE(flx_null_frame != nullptr);

    IChannelGroup* flx_error_frame = last_dg->GetChannelGroup("FLX_ErrorFrame");
    ASSERT_TRUE(flx_error_frame != nullptr);

    IChannelGroup* flx_symbol = last_dg->GetChannelGroup("FLX_Symbol");
    ASSERT_TRUE(flx_symbol != nullptr);

    writer->InitMeasurement();
    uint64_t tick_time = MdfHelper::NowNs();
    writer->StartMeasurement(tick_time);

    for (size_t sample = 0; sample < max_sample; ++sample) {

      FlexRayFrame d_frame;
      d_frame.DataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_frame, tick_time, d_frame);
      tick_time += 1'000'000;

      FlexRayErrorFrame e_frame;
      e_frame.DataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_error_frame, tick_time, e_frame);
      tick_time += 1'000'000;

      FlexRayPdu p_frame;
      p_frame.DataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_pdu, tick_time, p_frame);
      tick_time += 1'000'000;

      FlexRayFrameHeader f_frame;
      f_frame.FillDataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_frame_header, tick_time, f_frame);
      tick_time += 1'000'000;

      FlexRayNullFrame n_frame;
      n_frame.DataBytes(sample_data);
      writer->SaveFlexRayMessage(*last_dg, *flx_null_frame, tick_time, n_frame);
      tick_time += 1'000'000;

      FlexRaySymbol s_frame;
      writer->SaveFlexRayMessage(*last_dg, *flx_symbol, tick_time, s_frame);
      tick_time += 1'000'000;

    }

    writer->StopMeasurement(tick_time);
    writer->FinalizeMeasurement();
  }

  TestDataFile(mdf_file.wstring(), false, max_sample, sample_data);

  path sorted_file = mdf_file.parent_path();
  sorted_file /= mdf_file.stem();
  sorted_file += "_sorted";
  sorted_file += mdf_file.extension();
  {
    auto sortTask =
            mdf::MdfFactory::CreateTask(mdf::MdfTaskType::MdfSortingTask);

    sortTask->SourceFile(mdf_file.string());

    sortTask->DestinationFile(sorted_file.string());
    sortTask->SkipIfNoSamples(true);
    sortTask->Run();
  }
  TestDataFile(sorted_file.wstring(), true, max_sample, sample_data);

}

INSTANTIATE_TEST_SUITE_P(TestCanData, TestSortingData,
  testing::ValuesIn(kTestInputList));

INSTANTIATE_TEST_SUITE_P(TestLinData, TestSortingData,
  testing::ValuesIn(kTestInputList));

INSTANTIATE_TEST_SUITE_P(TestFlexRayData, TestSortingData,
  testing::ValuesIn(kTestInputList));
} // mdf::test