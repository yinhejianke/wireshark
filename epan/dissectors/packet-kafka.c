/* packet-kafka.c
 * Routines for Apache Kafka Protocol dissection (version 0.8 - 2.3)
 * Copyright 2013, Evan Huus <eapache@gmail.com>
 * Update from Kafka 0.10.1.0 to 2.3 by Piotr Smolinski <piotr.smolinski@confluent.io>
 *
 * https://cwiki.apache.org/confluence/display/KAFKA/A+Guide+To+The+Kafka+Protocol
 * https://kafka.apache.org/protocol.html
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/expert.h>
#include <epan/prefs.h>
#include <epan/proto_data.h>
#ifdef HAVE_SNAPPY
#include <snappy-c.h>
#endif
#ifdef HAVE_LZ4FRAME_H
#include <lz4.h>
#include <lz4frame.h>
#endif
#ifdef HAVE_ZSTD
#include <zstd.h>
#endif
#include "packet-tcp.h"
#include "packet-tls.h"

void proto_register_kafka(void);
void proto_reg_handoff_kafka(void);

static int proto_kafka = -1;
static dissector_handle_t kafka_handle;

static int hf_kafka_len = -1;
static int hf_kafka_api_key = -1;
static int hf_kafka_api_version = -1;
static int hf_kafka_request_api_key = -1;
static int hf_kafka_response_api_key = -1;
static int hf_kafka_request_api_version = -1;
static int hf_kafka_response_api_version = -1;
static int hf_kafka_correlation_id = -1;
static int hf_kafka_client_id = -1;
static int hf_kafka_client_host = -1;
static int hf_kafka_string_len = -1;
static int hf_kafka_bytes_len = -1;
static int hf_kafka_required_acks = -1;
static int hf_kafka_timeout = -1;
static int hf_kafka_topic_name = -1;
static int hf_kafka_transactional_id = -1;
static int hf_kafka_transaction_result = -1;
static int hf_kafka_transaction_timeout = -1;
static int hf_kafka_partition_id = -1;
static int hf_kafka_replica = -1;
static int hf_kafka_replication_factor = -1;
static int hf_kafka_isr = -1;
static int hf_kafka_offline = -1;
static int hf_kafka_last_stable_offset = -1;
static int hf_kafka_log_start_offset = -1;
static int hf_kafka_first_offset = -1;
static int hf_kafka_producer_id = -1;
static int hf_kafka_producer_epoch = -1;
static int hf_kafka_message_size = -1;
static int hf_kafka_message_crc = -1;
static int hf_kafka_message_magic = -1;
static int hf_kafka_message_codec = -1;
static int hf_kafka_message_timestamp_type = -1;
static int hf_kafka_message_timestamp = -1;
static int hf_kafka_batch_crc = -1;
static int hf_kafka_batch_codec = -1;
static int hf_kafka_batch_timestamp_type = -1;
static int hf_kafka_batch_transactional = -1;
static int hf_kafka_batch_control_batch = -1;
static int hf_kafka_batch_last_offset_delta = -1;
static int hf_kafka_batch_first_timestamp = -1;
static int hf_kafka_batch_last_timestamp = -1;
static int hf_kafka_batch_base_sequence = -1;
static int hf_kafka_batch_size = -1;
static int hf_kafka_message_key = -1;
static int hf_kafka_message_value = -1;
static int hf_kafka_message_compression_reduction = -1;
static int hf_kafka_request_frame = -1;
static int hf_kafka_response_frame = -1;
static int hf_kafka_consumer_group = -1;
static int hf_kafka_consumer_group_instance = -1;
static int hf_kafka_coordinator_key = -1;
static int hf_kafka_coordinator_type = -1;
static int hf_kafka_group_state = -1;
static int hf_kafka_offset = -1;
static int hf_kafka_offset_time = -1;
static int hf_kafka_max_offsets = -1;
static int hf_kafka_metadata = -1;
static int hf_kafka_error = -1;
static int hf_kafka_error_message = -1;
static int hf_kafka_broker_nodeid = -1;
static int hf_kafka_broker_epoch = -1;
static int hf_kafka_broker_host = -1;
static int hf_kafka_listener_name = -1;
static int hf_kafka_broker_port = -1;
static int hf_kafka_rack = -1;
static int hf_kafka_broker_security_protocol_type = -1;
static int hf_kafka_cluster_id = -1;
static int hf_kafka_controller_id = -1;
static int hf_kafka_controller_epoch = -1;
static int hf_kafka_delete_partitions = -1;
static int hf_kafka_leader_id = -1;
static int hf_kafka_group_leader_id = -1;
static int hf_kafka_leader_epoch = -1;
static int hf_kafka_current_leader_epoch = -1;
static int hf_kafka_is_internal = -1;
static int hf_kafka_isolation_level = -1;
static int hf_kafka_min_bytes = -1;
static int hf_kafka_max_bytes = -1;
static int hf_kafka_max_wait_time = -1;
static int hf_kafka_throttle_time = -1;
static int hf_kafka_api_versions_api_key = -1;
static int hf_kafka_api_versions_min_version = -1;
static int hf_kafka_api_versions_max_version = -1;
static int hf_kafka_session_timeout = -1;
static int hf_kafka_rebalance_timeout = -1;
static int hf_kafka_member_id = -1;
static int hf_kafka_protocol_type = -1;
static int hf_kafka_protocol_name = -1;
static int hf_kafka_protocol_metadata = -1;
static int hf_kafka_member_metadata = -1;
static int hf_kafka_generation_id = -1;
static int hf_kafka_member_assignment = -1;
static int hf_kafka_sasl_mechanism = -1;
static int hf_kafka_num_partitions = -1;
static int hf_kafka_zk_version = -1;
static int hf_kafka_config_key = -1;
static int hf_kafka_config_value = -1;
static int hf_kafka_commit_timestamp = -1;
static int hf_kafka_retention_time = -1;
static int hf_kafka_forgotten_topic_name = -1;
static int hf_kafka_forgotten_topic_partition = -1;
static int hf_kafka_fetch_session_id = -1;
static int hf_kafka_fetch_session_epoch = -1;
static int hf_kafka_record_header_key = -1;
static int hf_kafka_record_header_value = -1;
static int hf_kafka_record_attributes = -1;
static int hf_kafka_allow_auto_topic_creation = -1;
static int hf_kafka_validate_only = -1;
static int hf_kafka_coordinator_epoch = -1;
static int hf_kafka_sasl_auth_bytes = -1;
static int hf_kafka_session_lifetime_ms = -1;
static int hf_kafka_acl_resource_type = -1;
static int hf_kafka_acl_resource_name = -1;
static int hf_kafka_acl_resource_pattern_type = -1;
static int hf_kafka_acl_principal = -1;
static int hf_kafka_acl_host = -1;
static int hf_kafka_acl_operation = -1;
static int hf_kafka_acl_permission_type = -1;
static int hf_kafka_config_resource_type = -1;
static int hf_kafka_config_resource_name = -1;
static int hf_kafka_config_include_synonyms = -1;
static int hf_kafka_config_source = -1;
static int hf_kafka_config_readonly = -1;
static int hf_kafka_config_default = -1;
static int hf_kafka_config_sensitive = -1;
static int hf_kafka_config_operation = -1;
static int hf_kafka_log_dir = -1;
static int hf_kafka_segment_size = -1;
static int hf_kafka_offset_lag = -1;
static int hf_kafka_future = -1;
static int hf_kafka_partition_count = -1;
static int hf_kafka_token_max_life_time = -1;
static int hf_kafka_token_renew_time = -1;
static int hf_kafka_token_expiry_time = -1;
static int hf_kafka_token_principal_type = -1;
static int hf_kafka_token_principal_name = -1;
static int hf_kafka_token_issue_timestamp = -1;
static int hf_kafka_token_expiry_timestamp = -1;
static int hf_kafka_token_max_timestamp = -1;
static int hf_kafka_token_id = -1;
static int hf_kafka_token_hmac = -1;
static int hf_kafka_include_cluster_authorized_ops = -1;
static int hf_kafka_include_topic_authorized_ops = -1;
static int hf_kafka_include_group_authorized_ops = -1;
static int hf_kafka_cluster_authorized_ops = -1;
static int hf_kafka_topic_authorized_ops = -1;
static int hf_kafka_group_authorized_ops = -1;
static int hf_kafka_election_type = -1;

static int ett_kafka = -1;
static int ett_kafka_batch = -1;
static int ett_kafka_message = -1;
static int ett_kafka_message_set = -1;
static int ett_kafka_replicas = -1;
static int ett_kafka_isrs = -1;
static int ett_kafka_offline = -1;
static int ett_kafka_broker = -1;
static int ett_kafka_brokers = -1;
static int ett_kafka_broker_end_point = -1;
static int ett_kafka_markers = -1;
static int ett_kafka_marker = -1;
static int ett_kafka_topics = -1;
static int ett_kafka_topic = -1;
static int ett_kafka_partitions = -1;
static int ett_kafka_partition = -1;
static int ett_kafka_api_version = -1;
static int ett_kafka_group_protocols = -1;
static int ett_kafka_group_protocol = -1;
static int ett_kafka_group_members = -1;
static int ett_kafka_group_member = -1;
static int ett_kafka_group_assignments = -1;
static int ett_kafka_group_assignment = -1;
static int ett_kafka_groups = -1;
static int ett_kafka_group = -1;
static int ett_kafka_sasl_enabled_mechanisms = -1;
static int ett_kafka_replica_assignment = -1;
static int ett_kafka_configs = -1;
static int ett_kafka_config = -1;
static int ett_kafka_request_forgotten_topic = -1;
static int ett_kafka_record = -1;
static int ett_kafka_record_headers = -1;
static int ett_kafka_record_headers_header = -1;
static int ett_kafka_aborted_transactions = -1;
static int ett_kafka_aborted_transaction = -1;
static int ett_kafka_resources = -1;
static int ett_kafka_resource = -1;
static int ett_kafka_acls = -1;
static int ett_kafka_acl = -1;
static int ett_kafka_acl_creations = -1;
static int ett_kafka_acl_creation = -1;
static int ett_kafka_acl_filters = -1;
static int ett_kafka_acl_filter = -1;
static int ett_kafka_acl_filter_matches = -1;
static int ett_kafka_acl_filter_match = -1;
static int ett_kafka_config_synonyms = -1;
static int ett_kafka_config_synonym = -1;
static int ett_kafka_config_entries = -1;
static int ett_kafka_config_entry = -1;
static int ett_kafka_log_dirs = -1;
static int ett_kafka_log_dir = -1;
static int ett_kafka_renewers = -1;
static int ett_kafka_renewer = -1;
static int ett_kafka_owners = -1;
static int ett_kafka_owner = -1;
static int ett_kafka_tokens = -1;
static int ett_kafka_token = -1;

static expert_field ei_kafka_request_missing = EI_INIT;
static expert_field ei_kafka_unknown_api_key = EI_INIT;
static expert_field ei_kafka_unsupported_api_version = EI_INIT;
static expert_field ei_kafka_bad_string_length = EI_INIT;
static expert_field ei_kafka_bad_bytes_length = EI_INIT;
static expert_field ei_kafka_bad_array_length = EI_INIT;
static expert_field ei_kafka_bad_record_length = EI_INIT;
static expert_field ei_kafka_bad_varint = EI_INIT;
static expert_field ei_kafka_bad_message_set_length = EI_INIT;
static expert_field ei_kafka_unknown_message_magic = EI_INIT;

typedef gint16 kafka_api_key_t;
typedef gint16 kafka_api_version_t;
typedef gint16 kafka_error_t;
typedef gint32 kafka_partition_t;
typedef gint64 kafka_offset_t;

typedef struct _kafka_api_info_t {
    kafka_api_key_t api_key;
    const char *name;
    /* If api key is not supported then set min_version and max_version to -1 */
    kafka_api_version_t min_version;
    kafka_api_version_t max_version;
} kafka_api_info_t;

#define KAFKA_TCP_DEFAULT_RANGE     "9092"

#define KAFKA_PRODUCE                        0
#define KAFKA_FETCH                          1
#define KAFKA_OFFSETS                        2
#define KAFKA_METADATA                       3
#define KAFKA_LEADER_AND_ISR                 4
#define KAFKA_STOP_REPLICA                   5
#define KAFKA_UPDATE_METADATA                6
#define KAFKA_CONTROLLED_SHUTDOWN            7
#define KAFKA_OFFSET_COMMIT                  8
#define KAFKA_OFFSET_FETCH                   9
#define KAFKA_FIND_COORDINATOR              10
#define KAFKA_JOIN_GROUP                    11
#define KAFKA_HEARTBEAT                     12
#define KAFKA_LEAVE_GROUP                   13
#define KAFKA_SYNC_GROUP                    14
#define KAFKA_DESCRIBE_GROUPS               15
#define KAFKA_LIST_GROUPS                   16
#define KAFKA_SASL_HANDSHAKE                17
#define KAFKA_API_VERSIONS                  18
#define KAFKA_CREATE_TOPICS                 19
#define KAFKA_DELETE_TOPICS                 20
#define KAFKA_DELETE_RECORDS                21
#define KAFKA_INIT_PRODUCER_ID              22
#define KAFKA_OFFSET_FOR_LEADER_EPOCH       23
#define KAFKA_ADD_PARTITIONS_TO_TXN         24
#define KAFKA_ADD_OFFSETS_TO_TXN            25
#define KAFKA_END_TXN                       26
#define KAFKA_WRITE_TXN_MARKERS             27
#define KAFKA_TXN_OFFSET_COMMIT             28
#define KAFKA_DESCRIBE_ACLS                 29
#define KAFKA_CREATE_ACLS                   30
#define KAFKA_DELETE_ACLS                   31
#define KAFKA_DESCRIBE_CONFIGS              32
#define KAFKA_ALTER_CONFIGS                 33
#define KAFKA_ALTER_REPLICA_LOG_DIRS        34
#define KAFKA_DESCRIBE_LOG_DIRS             35
#define KAFKA_SASL_AUTHENTICATE             36
#define KAFKA_CREATE_PARTITIONS             37
#define KAFKA_CREATE_DELEGATION_TOKEN       38
#define KAFKA_RENEW_DELEGATION_TOKEN        39
#define KAFKA_EXPIRE_DELEGATION_TOKEN       40
#define KAFKA_DESCRIBE_DELEGATION_TOKEN     41
#define KAFKA_DELETE_GROUPS                 42
#define KAFKA_ELECT_LEADERS                 43
#define KAFKA_INC_ALTER_CONFIGS             44
#define KAFKA_ALTER_PARTITION_REASSIGNMENTS 45
#define KAFKA_LIST_PARTITION_REASSIGNMENTS  46

/*
 * Check for message changes here:
 * https://github.com/apache/kafka/tree/trunk/clients/src/main/resources/common/message
 */
static const kafka_api_info_t kafka_apis[] = {
    { KAFKA_PRODUCE,                       "Produce",
      0, 7 },
    { KAFKA_FETCH,                         "Fetch",
      0, 11 },
    { KAFKA_OFFSETS,                       "Offsets",
      0, 5 },
    { KAFKA_METADATA,                      "Metadata",
      0, 8 },
    { KAFKA_LEADER_AND_ISR,                "LeaderAndIsr",
      0, 3 },
    { KAFKA_STOP_REPLICA,                  "StopReplica",
      0, 1 },
    { KAFKA_UPDATE_METADATA,               "UpdateMetadata",
      0, 5 },
    { KAFKA_CONTROLLED_SHUTDOWN,           "ControlledShutdown",
      0, 2 },
    { KAFKA_OFFSET_COMMIT,                 "OffsetCommit",
      0, 7 },
    { KAFKA_OFFSET_FETCH,                  "OffsetFetch",
      0, 5 },
    { KAFKA_FIND_COORDINATOR,              "FindCoordinator",
      0, 2 },
    { KAFKA_JOIN_GROUP,                    "JoinGroup",
      0, 5 },
    { KAFKA_HEARTBEAT,                     "Heartbeat",
      0, 3 },
    { KAFKA_LEAVE_GROUP,                   "LeaveGroup",
      0, 3 },
    { KAFKA_SYNC_GROUP,                    "SyncGroup",
      0, 3 },
    { KAFKA_DESCRIBE_GROUPS,               "DescribeGroups",
      0, 4 },
    { KAFKA_LIST_GROUPS,                   "ListGroups",
      0, 2 },
    { KAFKA_SASL_HANDSHAKE,                "SaslHandshake",
      0, 1 },
    { KAFKA_API_VERSIONS,                  "ApiVersions",
      0, 2 },
    { KAFKA_CREATE_TOPICS,                 "CreateTopics",
      0, 4 },
    { KAFKA_DELETE_TOPICS,                 "DeleteTopics",
      0, 3 },
    { KAFKA_DELETE_RECORDS,                "DeleteRecords",
      0, 1 },
    { KAFKA_INIT_PRODUCER_ID,              "InitProducerId",
      0, 1 },
    { KAFKA_OFFSET_FOR_LEADER_EPOCH,       "OffsetForLeaderEpoch",
      0, 3 },
    { KAFKA_ADD_PARTITIONS_TO_TXN,         "AddPartitionsToTxn",
      0, 1 },
    { KAFKA_ADD_OFFSETS_TO_TXN,            "AddOffsetsToTxn",
      0, 1 },
    { KAFKA_END_TXN,                       "EndTxn",
      0, 1 },
    { KAFKA_WRITE_TXN_MARKERS,             "WriteTxnMarkers",
      0, 0 },
    { KAFKA_TXN_OFFSET_COMMIT,             "TxnOffsetCommit",
      0, 2 },
    { KAFKA_DESCRIBE_ACLS,                 "DescribeAcls",
      0, 1 },
    { KAFKA_CREATE_ACLS,                   "CreateAcls",
      0, 1 },
    { KAFKA_DELETE_ACLS,                   "DeleteAcls",
      0, 1 },
    { KAFKA_DESCRIBE_CONFIGS,              "DescribeConfigs",
      0, 2 },
    { KAFKA_ALTER_CONFIGS,                 "AlterConfigs",
      0, 1 },
    { KAFKA_ALTER_REPLICA_LOG_DIRS,        "AlterReplicaLogDirs",
      0, 1 },
    { KAFKA_DESCRIBE_LOG_DIRS,             "DescribeLogDirs",
      0, 1 },
    { KAFKA_SASL_AUTHENTICATE,             "SaslAuthenticate",
      0, 1 },
    { KAFKA_CREATE_PARTITIONS,             "CreatePartitions",
      0, 1 },
    { KAFKA_CREATE_DELEGATION_TOKEN,       "CreateDelegationToken",
      0, 1 },
    { KAFKA_RENEW_DELEGATION_TOKEN,        "RenewDelegationToken",
      0, 1 },
    { KAFKA_EXPIRE_DELEGATION_TOKEN,       "ExpireDelegationToken",
      0, 1 },
    { KAFKA_DESCRIBE_DELEGATION_TOKEN,     "DescribeDelegationToken",
      0, 1 },
    { KAFKA_DELETE_GROUPS,                 "DeleteGroups",
      0, 1 },
    { KAFKA_ELECT_LEADERS,                 "ElectLeaders",
      0, 1 },
    { KAFKA_INC_ALTER_CONFIGS,             "IncrementalAlterConfigs",
      0, 0 },
    { KAFKA_ALTER_PARTITION_REASSIGNMENTS, "AlterPartitionReassignments",
      0, 0 },
    { KAFKA_LIST_PARTITION_REASSIGNMENTS,  "ListPartitionReassignments",
      0, 0 },
};

/*
 * Generated from kafka_apis. Add 1 to length for last dummy element.
 */
static value_string kafka_api_names[array_length(kafka_apis) + 1];

/*
 * For the current list of error codes check here:
 * https://github.com/apache/kafka/blob/trunk/clients/src/main/java/org/apache/kafka/common/protocol/Errors.java
 */
static const value_string kafka_errors[] = {
    { -1, "Unexpected Server Error" },
    { 0, "No Error" },
    { 1, "Offset Out Of Range" },
    { 2, "Invalid Message" },
    { 3, "Unknown Topic or Partition" },
    { 4, "Invalid Message Size" },
    { 5, "Leader Not Available" },
    { 6, "Not Leader For Partition" },
    { 7, "Request Timed Out" },
    { 8, "Broker Not Available" },
    { 10, "Message Size Too Large" },
    { 11, "Stale Controller Epoch Code" },
    { 12, "Offset Metadata Too Large" },
    { 14, "Offsets Load In Progress" },
    { 15, "The Coordinator is not Available" },
    { 16, "Not Coordinator For Consumer" },
    { 17, "Invalid topic" },
    { 18, "Message batch larger than configured server segment size" },
    { 19, "Not enough in-sync replicas" },
    { 20, "Message(s) written to insufficient number of in-sync replicas" },
    { 21, "Invalid required acks value" },
    { 22, "Specified group generation id is not valid" },
    { 23, "Inconsistent group protocol" },
    { 24, "Invalid group.id" },
    { 25, "Unknown member" },
    { 26, "Invalid session timeout" },
    { 27, "Group rebalance in progress" },
    { 28, "Commit offset data size is not valid" },
    { 29, "Topic authorization failed" },
    { 30, "Group authorization failed" },
    { 31, "Cluster authorization failed" },
    { 32, "Invalid timestamp" },
    { 33, "Unsupported SASL mechanism" },
    { 34, "Illegal SASL state" },
    { 35, "Unsupported version" },
    { 36, "Topic already exists" },
    { 37, "Invalid number of partitions" },
    { 38, "Invalid replication-factor" },
    { 39, "Invalid replica assignment" },
    { 40, "Invalid configuration" },
    { 41, "Not controller" },
    { 42, "Invalid request" },
    { 43, "Unsupported for Message Format" },
    { 44, "Policy Violation" },
    { 45, "Out of Order Sequence Number" },
    { 46, "Duplicate Sequence Number" },
    { 47, "Invalid Producer Epoch" },
    { 48, "Invalid Transaction State" },
    { 49, "Invalid Producer ID Mapping" },
    { 50, "Invalid Transaction Timeout" },
    { 51, "Concurrent Transactions" },
    { 52, "Transaction Coordinator Fenced" },
    { 53, "Transactional ID Authorization Failed" },
    { 54, "Security Disabled" },
    { 55, "Operation not Attempted" },
    { 56, "Kafka Storage Error" },
    { 57, "Log Directory not Found" },
    { 58, "SASL Authentication failed" },
    { 59, "Unknown Producer ID" },
    { 60, "Partition Reassignment in Progress" },
    { 61, "Delegation Token Auth Disabled" },
    { 62, "Delegation Token not Found" },
    { 63, "Delegation Token Owner Mismatch" },
    { 64, "Delegation Token Request not Allowed" },
    { 65, "Delegation Token Authorization Failed" },
    { 66, "Delegation Token Expired" },
    { 67, "Supplied Principal Type Unsupported" },
    { 68, "Not Empty Group" },
    { 69, "Group ID not Found" },
    { 70, "Fetch Session ID not Found" },
    { 71, "Invalid Fetch Session Epoch" },
    { 72, "Listener not Found" },
    { 73, "Topic Deletion Disabled" },
    { 74, "Fenced Leader Epoch" },
    { 75, "Unknown Leader Epoch" },
    { 76, "Unsupported Compression Type" },
    { 77, "Stale Broker Epoch" },
    { 78, "Offset not Available" },
    { 79, "Member ID Required" },
    { 80, "Preferred Leader not Available" },
    { 81, "Group Max Size Reached" },
    { 82, "Fenced Instance ID" },
    { 83, "Eligible topic partition leaders are not available" },
    { 84, "Leader election not needed for topic partition" },
    { 85, "No partition reassignment is in progress" },
    { 0, NULL }
};

#define KAFKA_ACK_NOT_REQUIRED 0
#define KAFKA_ACK_LEADER       1
#define KAFKA_ACK_FULL_ISR     -1
static const value_string kafka_acks[] = {
    { KAFKA_ACK_NOT_REQUIRED, "Not Required" },
    { KAFKA_ACK_LEADER,       "Leader"       },
    { KAFKA_ACK_FULL_ISR,     "Full ISR"     },
    { 0, NULL }
};

#define KAFKA_MESSAGE_CODEC_MASK   0x07
#define KAFKA_MESSAGE_CODEC_NONE   0
#define KAFKA_MESSAGE_CODEC_GZIP   1
#define KAFKA_MESSAGE_CODEC_SNAPPY 2
#define KAFKA_MESSAGE_CODEC_LZ4    3
#define KAFKA_MESSAGE_CODEC_ZSTD   4
static const value_string kafka_message_codecs[] = {
    { KAFKA_MESSAGE_CODEC_NONE,   "None"   },
    { KAFKA_MESSAGE_CODEC_GZIP,   "Gzip"   },
    { KAFKA_MESSAGE_CODEC_SNAPPY, "Snappy" },
    { KAFKA_MESSAGE_CODEC_LZ4,    "LZ4"    },
    { KAFKA_MESSAGE_CODEC_ZSTD,   "Zstd"   },
    { 0, NULL }
};
#ifdef HAVE_SNAPPY
static const guint8 kafka_xerial_header[8] = {0x82, 0x53, 0x4e, 0x41, 0x50, 0x50, 0x59, 0x00};
#endif

#define KAFKA_MESSAGE_TIMESTAMP_MASK 0x08
static const value_string kafka_message_timestamp_types[] = {
    { 0, "CreateTime" },
    { 1, "LogAppendTime" },
    { 0, NULL }
};

#define KAFKA_BATCH_TRANSACTIONAL_MASK 0x10
static const value_string kafka_batch_transactional_values[] = {
    { 0, "Non-transactional" },
    { 1, "Transactional" },
    { 0, NULL }
};

#define KAFKA_BATCH_CONTROL_BATCH_MASK 0x20
static const value_string kafka_batch_control_batch_values[] = {
    { 0, "Data batch" },
    { 1, "Control batch" },
    { 0, NULL }
};

static const value_string kafka_coordinator_types[] = {
    { 0, "Group" },
    { 1, "Transaction" },
    { 0, NULL }
};

static const value_string kafka_security_protocol_types[] = {
    { 0, "PLAINTEXT" },
    { 1, "SSL" },
    { 2, "SASL_PLAINTEXT" },
    { 3, "SASL_SSL" },
    { 0, NULL }
};

static const value_string kafka_isolation_levels[] = {
    { 0, "Read Uncommitted" },
    { 1, "Read Committed" },
    { 0, NULL }
};

static const value_string kafka_transaction_results[] = {
    { 0, "ABORT" },
    { 1, "COMMIT" },
    { 0, NULL }
};

static const value_string acl_resource_types[] = {
    { 0, "Unknown" },
    { 1, "Any" },
    { 2, "Topic" },
    { 3, "Group" },
    { 4, "Cluster" },
    { 5, "TransactionalId" },
    { 6, "DelegationToken" },
    { 0, NULL }
};

static const value_string acl_resource_pattern_types[] = {
    { 0, "Unknown" },
    { 1, "Any" },
    { 2, "Match" },
    { 3, "Literal" },
    { 4, "Prefixed" },
    { 0, NULL }
};

static const value_string acl_operations[] = {
    { 0, "Unknown" },
    { 1, "Any" },
    { 2, "All" },
    { 3, "Read" },
    { 4, "Write" },
    { 5, "Create" },
    { 6, "Delete" },
    { 7, "Alter" },
    { 8, "Describe" },
    { 9, "Cluster Action" },
    { 10, "Describe Configs" },
    { 11, "Alter Configs" },
    { 12, "Idempotent Write" },
    { 0, NULL }
};

static const value_string acl_permission_types[] = {
    { 0, "Unknown" },
    { 1, "Any" },
    { 2, "Deny" },
    { 3, "Allow" },
    { 0, NULL }
};

static const value_string config_resource_types[] = {
    { 0, "Unknown" },
    { 2, "Topic" },
    { 4, "Broker" },
    { 0, NULL }
};

static const value_string config_sources[] = {
    { 0, "Unknown" },
    { 1, "Topic" },
    { 2, "Broker (Dynamic)" },
    { 3, "Broker (Dynamic/Default)" },
    { 4, "Broker (Static)" },
    { 5, "Default" },
    { 0, NULL }
};

static const value_string config_operations[] = {
    { 0, "Set" },
    { 1, "Delete" },
    { 2, "Append" },
    { 3, "Subtract" },
    { 0, NULL }
};

static const value_string election_types[] = {
    { 0, "Preferred" },
    { 1, "Unclean" },
    { 0, NULL }
};

/* Whether to show the lengths of string and byte fields in the protocol tree.
 * It can be useful to see these, but they do clutter up the display, so disable
 * by default */
static gboolean kafka_show_string_bytes_lengths = FALSE;

typedef struct _kafka_query_response_t {
    kafka_api_key_t     api_key;
    kafka_api_version_t api_version;
    guint32  request_frame;
    guint32  response_frame;
    gboolean response_found;
} kafka_query_response_t;


/* Some values to temporarily remember during dissection */
typedef struct kafka_packet_values_t {
    kafka_partition_t partition_id;
    kafka_offset_t    offset;
} kafka_packet_values_t;

/* Forward declaration (dissect_kafka_message_set() and dissect_kafka_message() call each other...) */
static int
dissect_kafka_message_set(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, gint offset, guint len, guint8 codec);


/* HELPERS */

#ifdef HAVE_LZ4FRAME_H
/* Local copy of XXH32() algorithm as found in https://github.com/lz4/lz4/blob/v1.7.5/lib/xxhash.c
   as some packagers are not providing xxhash.h in liblz4 */
typedef struct {
    guint32 total_len_32;
    guint32 large_len;
    guint32 v1;
    guint32 v2;
    guint32 v3;
    guint32 v4;
    guint32 mem32[4];   /* buffer defined as U32 for alignment */
    guint32 memsize;
    guint32 reserved;   /* never read nor write, will be removed in a future version */
} XXH32_state_t;

typedef enum {
    XXH_bigEndian=0,
    XXH_littleEndian=1
} XXH_endianess;

static const int g_one = 1;
#define XXH_CPU_LITTLE_ENDIAN   (*(const char*)(&g_one))

static const guint32 PRIME32_1 = 2654435761U;
static const guint32 PRIME32_2 = 2246822519U;
static const guint32 PRIME32_3 = 3266489917U;
static const guint32 PRIME32_4 =  668265263U;
static const guint32 PRIME32_5 =  374761393U;

#define XXH_rotl32(x,r) ((x << r) | (x >> (32 - r)))

static guint32 XXH_read32(const void* memPtr)
{
    guint32 val;
    memcpy(&val, memPtr, sizeof(val));
    return val;
}

static guint32 XXH_swap32(guint32 x)
{
    return  ((x << 24) & 0xff000000 ) |
            ((x <<  8) & 0x00ff0000 ) |
            ((x >>  8) & 0x0000ff00 ) |
            ((x >> 24) & 0x000000ff );
}

#define XXH_readLE32(ptr, endian) (endian==XXH_littleEndian ? XXH_read32(ptr) : XXH_swap32(XXH_read32(ptr)))

static guint32 XXH32_round(guint32 seed, guint32 input)
{
    seed += input * PRIME32_2;
    seed  = XXH_rotl32(seed, 13);
    seed *= PRIME32_1;
    return seed;
}

static guint32 XXH32_endian(const void* input, size_t len, guint32 seed, XXH_endianess endian)
{
    const gint8* p = (const gint8*)input;
    const gint8* bEnd = p + len;
    guint32 h32;
#define XXH_get32bits(p) XXH_readLE32(p, endian)

    if (len>=16) {
        const gint8* const limit = bEnd - 16;
        guint32 v1 = seed + PRIME32_1 + PRIME32_2;
        guint32 v2 = seed + PRIME32_2;
        guint32 v3 = seed + 0;
        guint32 v4 = seed - PRIME32_1;

        do {
            v1 = XXH32_round(v1, XXH_get32bits(p)); p+=4;
            v2 = XXH32_round(v2, XXH_get32bits(p)); p+=4;
            v3 = XXH32_round(v3, XXH_get32bits(p)); p+=4;
            v4 = XXH32_round(v4, XXH_get32bits(p)); p+=4;
        } while (p<=limit);

        h32 = XXH_rotl32(v1, 1) + XXH_rotl32(v2, 7) + XXH_rotl32(v3, 12) + XXH_rotl32(v4, 18);
    } else {
        h32  = seed + PRIME32_5;
    }

    h32 += (guint32) len;

    while (p+4<=bEnd) {
        h32 += XXH_get32bits(p) * PRIME32_3;
        h32  = XXH_rotl32(h32, 17) * PRIME32_4 ;
        p+=4;
    }

    while (p<bEnd) {
        h32 += (*p) * PRIME32_5;
        h32 = XXH_rotl32(h32, 11) * PRIME32_1 ;
        p++;
    }

    h32 ^= h32 >> 15;
    h32 *= PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= PRIME32_3;
    h32 ^= h32 >> 16;

    return h32;
}

static guint XXH32(const void* input, size_t len, guint seed)
{
    XXH_endianess endian_detected = (XXH_endianess)XXH_CPU_LITTLE_ENDIAN;
    if (endian_detected==XXH_littleEndian)
        return XXH32_endian(input, len, seed, XXH_littleEndian);
    else
        return XXH32_endian(input, len, seed, XXH_bigEndian);
}
#endif /* HAVE_LZ4FRAME_H */

static const char *
kafka_error_to_str(kafka_error_t error)
{
    return val_to_str(error, kafka_errors, "Unknown %d");
}

static const char *
kafka_api_key_to_str(kafka_api_key_t api_key)
{
    return val_to_str(api_key, kafka_api_names, "Unknown %d");
}

static const kafka_api_info_t *
kafka_get_api_info(kafka_api_key_t api_key)
{
    if ((api_key >= 0) && (api_key < ((kafka_api_key_t) array_length(kafka_apis)))) {
        return &kafka_apis[api_key];
    } else {
        return NULL;
    }
}

static gboolean
kafka_is_api_version_supported(const kafka_api_info_t *api_info, kafka_api_version_t api_version)
{
    DISSECTOR_ASSERT(api_info);

    return !(api_info->min_version == -1 ||
             api_version < api_info->min_version ||
             api_version > api_info->max_version);
}

static void
kafka_check_supported_api_key(packet_info *pinfo, proto_item *ti, kafka_query_response_t *matcher)
{
    if (kafka_get_api_info(matcher->api_key) == NULL) {
        col_append_str(pinfo->cinfo, COL_INFO, " [Unknown API key]");
        expert_add_info_format(pinfo, ti, &ei_kafka_unknown_api_key,
                               "%s API key", kafka_api_key_to_str(matcher->api_key));
    }
}

static void
kafka_check_supported_api_version(packet_info *pinfo, proto_item *ti, kafka_query_response_t *matcher)
{
    const kafka_api_info_t *api_info;

    api_info = kafka_get_api_info(matcher->api_key);
    if (api_info != NULL && !kafka_is_api_version_supported(api_info, matcher->api_version)) {
        col_append_str(pinfo->cinfo, COL_INFO, " [Unsupported API version]");
        if (api_info->min_version == -1) {
            expert_add_info_format(pinfo, ti, &ei_kafka_unsupported_api_version,
                                   "Unsupported %s version.",
                                   kafka_api_key_to_str(matcher->api_key));
        }
        else if (api_info->min_version == api_info->max_version) {
            expert_add_info_format(pinfo, ti, &ei_kafka_unsupported_api_version,
                                   "Unsupported %s version. Supports v%d.",
                                   kafka_api_key_to_str(matcher->api_key), api_info->min_version);
        } else {
            expert_add_info_format(pinfo, ti, &ei_kafka_unsupported_api_version,
                                   "Unsupported %s version. Supports v%d-%d.",
                                   kafka_api_key_to_str(matcher->api_key),
                                   api_info->min_version, api_info->max_version);
        }
    }
}

static guint
get_kafka_pdu_len(packet_info *pinfo _U_, tvbuff_t *tvb, int offset, void *data _U_)
{
    return 4 + tvb_get_ntohl(tvb, offset);
}

static int
dissect_kafka_array_ref(proto_tree *tree, tvbuff_t *tvb, packet_info *pinfo, int offset,
                    kafka_api_version_t api_version,
                    int(*func)(tvbuff_t*, packet_info*, proto_tree*, int, kafka_api_version_t),
                    int *p_count)
{
    gint32 count, i;

    count = (gint32) tvb_get_ntohl(tvb, offset);
    offset += 4;

    if (count < -1) { // -1 means null array
        expert_add_info(pinfo, proto_tree_get_parent(tree), &ei_kafka_bad_array_length);
    }
    else {
        for (i=0; i<count; i++) {
            offset = func(tvb, pinfo, tree, offset, api_version);
        }
    }

    if (p_count != NULL) {
        *p_count = count;
    }

    return offset;
}

static int
dissect_kafka_array(proto_tree *tree, tvbuff_t *tvb, packet_info *pinfo, int offset,
                    kafka_api_version_t api_version,
                    int(*func)(tvbuff_t*, packet_info*, proto_tree*, int, kafka_api_version_t))
{
    return dissect_kafka_array_ref(tree, tvb, pinfo, offset, api_version, func, NULL);
}

static int
dissect_kafka_string(proto_tree *tree, int hf_item, tvbuff_t *tvb, packet_info *pinfo, int offset,
                     int *p_string_offset, int *p_string_len)
{
    gint16 len;
    proto_item *pi;

    /* String length */
    len = (gint16) tvb_get_ntohs(tvb, offset);
    pi = proto_tree_add_item(tree, hf_kafka_string_len, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    if (p_string_offset != NULL) *p_string_offset = offset;

    if (len < -1) {
        expert_add_info(pinfo, pi, &ei_kafka_bad_string_length);
    }
    else {
        /* Only showing length field if preference indicates */
        if (!kafka_show_string_bytes_lengths) {
            proto_item_set_hidden(pi);
        }

        if (len == -1) {
            /* -1 indicates a NULL string */
            proto_tree_add_string(tree, hf_item, tvb, offset, 0, NULL);
        }
        else {
            /* Add the string itself. */
            proto_tree_add_item(tree, hf_item, tvb, offset, len, ENC_NA|ENC_ASCII);
            offset += len;
        }
    }

    if (p_string_len != NULL) *p_string_len = len;

    return offset;
}

static int
dissect_kafka_bytes(proto_tree *tree, int hf_item, tvbuff_t *tvb, packet_info *pinfo, int offset,
                    int *p_bytes_offset, int *p_bytes_len)
{
    gint32 len;
    proto_item *pi;

    /* Length */
    len = (gint32) tvb_get_ntohl(tvb, offset);
    pi = proto_tree_add_item(tree, hf_kafka_bytes_len, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (p_bytes_offset != NULL) *p_bytes_offset = offset;

    if (len < -1) {
        expert_add_info(pinfo, pi, &ei_kafka_bad_bytes_length);
    }
    else {
        /* Only showing length field if preference indicates */
        if (!kafka_show_string_bytes_lengths) {
            proto_item_set_hidden(pi);
        }

        if (len == -1) {
            proto_tree_add_bytes(tree, hf_item, tvb, offset, 0, NULL);
        }
        else {
            proto_tree_add_item(tree, hf_item, tvb, offset, len, ENC_NA);
            offset += len;
        }
    }

    if (p_bytes_len != NULL) *p_bytes_len = len;

    return offset;
}

static int
dissect_kafka_timestamp_delta(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int hf_item, int offset, guint64 first_timestamp)
{
    nstime_t   nstime;
    guint64    milliseconds;
    guint64    val;
    guint      len;
    proto_item *pi;

    len = tvb_get_varint(tvb, offset, FT_VARINT_MAX_LEN, &val, ENC_VARINT_ZIGZAG);

    milliseconds = first_timestamp + val;
    nstime.secs  = (time_t) (milliseconds / 1000);
    nstime.nsecs = (int) ((milliseconds % 1000) * 1000000);

    pi = proto_tree_add_time(tree, hf_item, tvb, offset, len, &nstime);
    if (len == 0) {
        //This will probably lead to a malformed packet, but it's better than not incrementing the offset
        len = FT_VARINT_MAX_LEN;
        expert_add_info(pinfo, pi, &ei_kafka_bad_varint);
    }

    return offset+len;
}

static int
dissect_kafka_offset_delta(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int hf_item, int offset, guint64 base_offset)
{
    gint64     val;
    guint      len;
    proto_item *pi;

    len = tvb_get_varint(tvb, offset, FT_VARINT_MAX_LEN, &val, ENC_VARINT_ZIGZAG);

    pi = proto_tree_add_int64(tree, hf_item, tvb, offset, len, base_offset+val);
    if (len == 0) {
        //This will probably lead to a malformed packet, but it's better than not incrementing the offset
        len = FT_VARINT_MAX_LEN;
        expert_add_info(pinfo, pi, &ei_kafka_bad_varint);
    }

    return offset+len;
}

/*
 * Function: dissect_kafka_string_new
 * ---------------------------------------------------
 * Decodes UTF string using the new length encoding. This format is used
 * in the v2 message encoding, where the string length is encoded using
 * ProtoBuf's ZigZag integer format (inspired by Avro). The main advantage
 * of ZigZag is very compact representation for small numbers.
 *
 * tvb: actual data buffer
 * pinfo: packet information (unused)
 * tree: protocol information tree to append the item
 * hf_item: protocol information item descriptor index
 * offset: offset in the buffer where the string length is to be found
 * p_string_offset: pointer to a variable to store the actual string begin
 * p_string_length: pointer to a variable to store the actual string length
 *
 * returns: pointer to the next field in the message
 */
static int
dissect_kafka_string_new(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int hf_item, int offset, int *p_string_offset, int *p_string_length)
{
    gint64 val;
    guint len;
    proto_item *pi;

    len = tvb_get_varint(tvb, offset, 5, &val, ENC_VARINT_ZIGZAG);

    if (len == 0) {
        pi = proto_tree_add_string_format_value(tree, hf_item, tvb, offset+len, 0, NULL, "<INVALID>");
        expert_add_info(pinfo, pi, &ei_kafka_bad_varint);
        len = 5;
        val = 0;
    } else if (val > 0) {
        // there is payload available, possibly with 0 octets
        proto_tree_add_item(tree, hf_item, tvb, offset+len, (gint)val, ENC_NA | ENC_UTF_8);
    } else if (val == 0) {
        // there is empty payload (0 octets)
        proto_tree_add_string_format_value(tree, hf_item, tvb, offset+len, 0, NULL, "<EMPTY>");
    } else if (val == -1) {
        // there is no payload (null)
        proto_tree_add_string_format_value(tree, hf_item, tvb, offset+len, 0, NULL, "<NULL>");
        val = 0;
    } else {
        pi = proto_tree_add_string_format_value(tree, hf_item, tvb, offset+len, 0, NULL, "<INVALID>");
        expert_add_info(pinfo, pi, &ei_kafka_bad_string_length);
        val = 0;
    }

    if (p_string_offset != NULL) {
        *p_string_offset = offset+len;
    }
    if (p_string_length != NULL) {
        *p_string_length = (gint)val;
    }

    return offset+len+(gint)val;
}

/*
 * Function: dissect_kafka_bytes_new
 * ---------------------------------------------------
 * Decodes byte buffer using the new length encoding. This format is used
 * in the v2 message encoding, where the buffer length is encoded using
 * ProtoBuf's ZigZag integer format (inspired by Avro). The main advantage
 * of ZigZag is very compact representation for small numbers.
 *
 * tvb: actual data buffer
 * pinfo: packet information (unused)
 * tree: protocol information tree to append the item
 * hf_item: protocol information item descriptor index
 * offset: offset in the buffer where the string length is to be found
 * p_bytes_offset: pointer to a variable to store the actual buffer begin
 * p_bytes_length: pointer to a variable to store the actual buffer length
 *
 * returns: pointer to the next field in the message
 */
static int
dissect_kafka_bytes_new(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int hf_item, int offset, int *p_bytes_offset, int *p_bytes_length)
{
    gint64     val;
    guint      len;
    proto_item *pi;

    len = tvb_get_varint(tvb, offset, 5, &val, ENC_VARINT_ZIGZAG);

    if (len == 0) {
        pi = proto_tree_add_bytes_format_value(tree, hf_item, tvb, offset+len, 0, NULL, "<INVALID>");
        expert_add_info(pinfo, pi, &ei_kafka_bad_varint);
        len = 5;
        val = 0;
    } else if (val > 0) {
        // there is payload available, possibly with 0 octets
        proto_tree_add_item(tree, hf_item, tvb, offset+len, (gint)val, ENC_NA);
    } else if (val == 0) {
        // there is empty payload (0 octets)
        proto_tree_add_bytes_format_value(tree, hf_item, tvb, offset+len, 0, NULL, "<EMPTY>");
    } else if (val == -1) {
        // there is no payload (null)
        proto_tree_add_bytes_format_value(tree, hf_item, tvb, offset+len, 0, NULL, "<NULL>");
        val = 0;
    } else {
        pi = proto_tree_add_bytes_format_value(tree, hf_item, tvb, offset+len, 0, NULL, "<INVALID>");
        expert_add_info(pinfo, pi, &ei_kafka_bad_bytes_length);
        val = 0;
    }

    if (p_bytes_offset != NULL) {
        *p_bytes_offset = offset+len;
    }
    if (p_bytes_length != NULL) {
        *p_bytes_length = (gint)val;
    }
    return offset+len+(gint)val;
}

/* Calculate and show the reduction in transmitted size due to compression */
static void
show_compression_reduction(tvbuff_t *tvb, proto_tree *tree, guint compressed_size, guint uncompressed_size)
{
    proto_item *ti;
    /* Not really expecting a message to compress down to nothing, but defend against dividing by 0 anyway */
    if (uncompressed_size != 0) {
        ti = proto_tree_add_float(tree, hf_kafka_message_compression_reduction, tvb, 0, 0,
                                  (float)compressed_size / (float)uncompressed_size);
        proto_item_set_generated(ti);
    }
}

static int
dissect_kafka_record_headers_header(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset)
{
    proto_item *header_ti;
    proto_tree *subtree;

    int key_off, key_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_record_headers_header, &header_ti, "Header");

    offset = dissect_kafka_string_new(tvb, pinfo, subtree, hf_kafka_record_header_key, offset, &key_off, &key_len);
    offset = dissect_kafka_bytes_new(tvb, pinfo, subtree, hf_kafka_record_header_value, offset, NULL, NULL);

    proto_item_append_text(header_ti, " (Key: %s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb, key_off, key_len, ENC_UTF_8));
    proto_item_set_end(header_ti, tvb, offset);

    return offset;
}

static int
dissect_kafka_record_headers(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset)
{
    proto_item *record_headers_ti;
    proto_tree *subtree;
    gint64     count;
    guint      len;
    int        i;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_record_headers, &record_headers_ti, "Headers");

    len = tvb_get_varint(tvb, offset, 5, &count, ENC_VARINT_ZIGZAG);
    if (len == 0) {
        expert_add_info(pinfo, record_headers_ti, &ei_kafka_bad_varint);
        len = 5;
    } else if (count < -1) { // -1 means null array
        expert_add_info(pinfo, record_headers_ti, &ei_kafka_bad_array_length);
    }

    offset += len;
    for (i=0;i<count;i++) {
        offset = dissect_kafka_record_headers_header(tvb, pinfo, subtree, offset);
    }

    proto_item_set_end(record_headers_ti, tvb, offset);

    return offset;
}

static int
dissect_kafka_record(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int start_offset, guint64 base_offset, guint64 first_timestamp)
{
    proto_item *record_ti;
    proto_tree *subtree;

    gint64     size;
    guint      len;

    int offset, end_offset;

    offset = start_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_record, &record_ti, "Record");

    len = tvb_get_varint(tvb, offset, 5, &size, ENC_VARINT_ZIGZAG);
    if (len == 0) {
        expert_add_info(pinfo, record_ti, &ei_kafka_bad_varint);
        return offset + 5;
    } else if (size < 6) {
        expert_add_info(pinfo, record_ti, &ei_kafka_bad_record_length);
        return offset + len;
    }

    end_offset = offset + len + (gint)size;
    offset += len;

    proto_tree_add_item(subtree, hf_kafka_record_attributes, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_timestamp_delta(tvb, pinfo, subtree, hf_kafka_message_timestamp, offset, first_timestamp);
    offset = dissect_kafka_offset_delta(tvb, pinfo, subtree, hf_kafka_offset, offset, base_offset);

    offset = dissect_kafka_bytes_new(tvb, pinfo, subtree, hf_kafka_message_key, offset, NULL, NULL);
    offset = dissect_kafka_bytes_new(tvb, pinfo, subtree, hf_kafka_message_value, offset, NULL, NULL);

    offset = dissect_kafka_record_headers(tvb, pinfo, subtree, offset);

    if (offset != end_offset) {
        expert_add_info(pinfo, record_ti, &ei_kafka_bad_record_length);
    }

    return end_offset;
}

static int
decompress_none(tvbuff_t *tvb, packet_info *pinfo _U_, int offset, int length _U_, tvbuff_t **decompressed_tvb, int *decompressed_offset)
{
    *decompressed_tvb = tvb;
    *decompressed_offset = offset;
    return 1;
}

static int
decompress_gzip(tvbuff_t *tvb, packet_info *pinfo, int offset, int length, tvbuff_t **decompressed_tvb, int *decompressed_offset)
{
    *decompressed_tvb = tvb_child_uncompress(tvb, tvb, offset, length);
    *decompressed_offset = 0;
    if (*decompressed_tvb) {
        return 1;
    } else {
        col_append_str(pinfo->cinfo, COL_INFO, " [gzip decompression failed] ");
        return 0;
    }
}

#ifdef HAVE_LZ4FRAME_H
static int
decompress_lz4(tvbuff_t *tvb, packet_info *pinfo, int offset, int length, tvbuff_t **decompressed_tvb, int *decompressed_offset)
{
    LZ4F_decompressionContext_t lz4_ctxt = NULL;
    LZ4F_frameInfo_t lz4_info;
    LZ4F_errorCode_t rc = 0;
    size_t src_offset = 0, src_size = 0, dst_size = 0;
    guchar *decompressed_buffer = NULL;
    tvbuff_t *composite_tvb = tvb_new_composite();

    int ret = 0;

    /* Prepare compressed data buffer */
    guint8 *data = (guint8*)tvb_memdup(wmem_packet_scope(), tvb, offset, length);
    /* Override header checksum to workaround buggy Kafka implementations */
    if (length > 7) {
        int hdr_end = 6;
        if (data[4] & 0x08) {
            hdr_end += 8;
        }
        if (hdr_end < length) {
            data[hdr_end] = (XXH32(&data[4], hdr_end - 4, 0) >> 8) & 0xff;
        }
    }

    /* Allocate output buffer */
    rc = LZ4F_createDecompressionContext(&lz4_ctxt, LZ4F_VERSION);
    if (LZ4F_isError(rc)) {
        goto end;
    }

    src_offset = length;
    rc = LZ4F_getFrameInfo(lz4_ctxt, &lz4_info, data, &src_offset);
    if (LZ4F_isError(rc)) {
        goto end;
    }

    switch (lz4_info.blockSizeID) {
        case LZ4F_max64KB:
            dst_size = 1 << 16;
            break;
        case LZ4F_max256KB:
            dst_size = 1 << 18;
            break;
        case LZ4F_max1MB:
            dst_size = 1 << 20;
            break;
        case LZ4F_max4MB:
            dst_size = 1 << 22;
            break;
        default:
            goto end;
    }

    if (lz4_info.contentSize && lz4_info.contentSize < dst_size) {
        dst_size = (size_t)lz4_info.contentSize;
    }

    do {
        src_size = length - src_offset; // set the number of available octets
        decompressed_buffer = (guchar*)wmem_alloc(pinfo->pool, dst_size);
        rc = LZ4F_decompress(lz4_ctxt, decompressed_buffer, &dst_size,
                              &data[src_offset], &src_size, NULL);
        if (LZ4F_isError(rc)) {
            goto end;
        }
        tvb_composite_append(composite_tvb,
                             tvb_new_child_real_data(tvb, (guint8*)decompressed_buffer, (guint)dst_size, (gint)dst_size));
        src_offset += src_size; // bump up the offset for the next iteration
    } while (rc > 0);

    tvb_composite_finalize(composite_tvb);
    *decompressed_tvb = composite_tvb;
    *decompressed_offset = 0;
    composite_tvb = NULL;
    ret = 1;
end:
    LZ4F_freeDecompressionContext(lz4_ctxt);
    if (composite_tvb != NULL) {
        tvb_free_chain(composite_tvb);
    }
    if (ret == 0) {
        col_append_str(pinfo->cinfo, COL_INFO, " [lz4 decompression failed]");
    }
    return ret;
}
#else
static int
decompress_lz4(tvbuff_t *tvb _U_, packet_info *pinfo, int offset _U_, int length _U_, tvbuff_t **decompressed_tvb _U_, int *decompressed_offset _U_)
{
    col_append_str(pinfo->cinfo, COL_INFO, " [lz4 decompression unsupported]");
    return 0;
}
#endif /* HAVE_LZ4FRAME_H */

#ifdef HAVE_SNAPPY
static int
decompress_snappy(tvbuff_t *tvb, packet_info *pinfo, int offset, int length, tvbuff_t **decompressed_tvb, int *decompressed_offset)
{
    guint8 *data = (guint8*)tvb_memdup(wmem_packet_scope(), tvb, offset, length);
    size_t uncompressed_size;
    snappy_status rc = SNAPPY_OK;
    tvbuff_t *composite_tvb = NULL;
    int ret = 0;

    if (tvb_memeql(tvb, offset, kafka_xerial_header, sizeof(kafka_xerial_header)) == 0) {

        /* xerial framing format */
        int chunk_size, pos = 16;

        composite_tvb = tvb_new_composite();

        while (pos < length) {
            chunk_size = tvb_get_ntohl(tvb, offset+pos);
            pos += 4;
            if (pos+chunk_size > length) {
                // fail when the declared chunk size does not fit into remaining content
                goto end;
            }
            rc = snappy_uncompressed_length(&data[pos], chunk_size, &uncompressed_size);
            if (rc != SNAPPY_OK) {
                goto end;
            }
            guint8 *decompressed_buffer = (guint8*)wmem_alloc(pinfo->pool, uncompressed_size);
            rc = snappy_uncompress(&data[pos], chunk_size, decompressed_buffer, &uncompressed_size);
            if (rc != SNAPPY_OK) {
                goto end;
            }
            tvb_composite_append(composite_tvb,
                      tvb_new_child_real_data(tvb, decompressed_buffer, (guint)uncompressed_size, (gint)uncompressed_size));
            pos += chunk_size;
        }

        tvb_composite_finalize(composite_tvb);
        *decompressed_tvb = composite_tvb;
        *decompressed_offset = 0;
        composite_tvb = NULL;

    } else {

        /* unframed format */
        rc = snappy_uncompressed_length(data, length, &uncompressed_size);
        if (rc != SNAPPY_OK) {
            goto end;
        }

        guint8 *decompressed_buffer = (guint8*)wmem_alloc(pinfo->pool, uncompressed_size);

        rc = snappy_uncompress(data, length, decompressed_buffer, &uncompressed_size);
        if (rc != SNAPPY_OK) {
            goto end;
        }

        *decompressed_tvb = tvb_new_child_real_data(tvb, decompressed_buffer, (guint)uncompressed_size, (gint)uncompressed_size);
        *decompressed_offset = 0;

    }
    ret = 1;
end:
    if (composite_tvb != NULL) {
        tvb_free_chain(composite_tvb);
    }
    if (ret == 0) {
        col_append_str(pinfo->cinfo, COL_INFO, " [snappy decompression failed]");
    }
    return ret;
}
#else
static int
decompress_snappy(tvbuff_t *tvb _U_, packet_info *pinfo, int offset _U_, int length _U_, tvbuff_t **decompressed_tvb _U_, int *decompressed_offset _U_)
{
    col_append_str(pinfo->cinfo, COL_INFO, " [snappy decompression unsupported]");
    return 0;
}
#endif /* HAVE_SNAPPY */

#ifdef HAVE_ZSTD
static int
decompress_zstd(tvbuff_t *tvb, packet_info *pinfo, int offset, int length, tvbuff_t **decompressed_tvb, int *decompressed_offset)
{
    ZSTD_inBuffer input = { tvb_memdup(wmem_packet_scope(), tvb, offset, length), length, 0 };
    ZSTD_DStream *zds = ZSTD_createDStream();
    size_t rc = 0;
    tvbuff_t *composite_tvb = tvb_new_composite();
    int ret = 0;

    do {
        ZSTD_outBuffer output = { wmem_alloc(pinfo->pool, ZSTD_DStreamOutSize()), ZSTD_DStreamOutSize(), 0 };
        rc = ZSTD_decompressStream(zds, &output, &input);
        // rc holds either the number of decompressed offsets or the error code.
        // Both values are positive, one has to use ZSTD_isError to determine if the call succeeded.
        if (ZSTD_isError(rc)) {
            goto end;
        }
        tvb_composite_append(composite_tvb,
                             tvb_new_child_real_data(tvb, (guint8*)output.dst, (guint)output.pos, (gint)output.pos));
        // rc == 0 means there is nothing more to decompress, but there could be still something in the data
    } while (rc > 0);
    tvb_composite_finalize(composite_tvb);
    *decompressed_tvb = composite_tvb;
    *decompressed_offset = 0;
    composite_tvb = NULL;
    ret = 1;
end:
    ZSTD_freeDStream(zds);
    if (composite_tvb != NULL) {
        tvb_free_chain(composite_tvb);
    }
    if (ret == 0) {
        col_append_str(pinfo->cinfo, COL_INFO, " [zstd decompression failed]");
    }
    return ret;
}
#else
static int
decompress_zstd(tvbuff_t *tvb _U_, packet_info *pinfo, int offset _U_, int length _U_, tvbuff_t **decompressed_tvb _U_, int *decompressed_offset _U_)
{
    col_append_str(pinfo->cinfo, COL_INFO, " [zstd compression unsupported]");
    return 0;
}
#endif /* HAVE_ZSTD */

static int
decompress(tvbuff_t *tvb, packet_info *pinfo, int offset, int length, int codec, tvbuff_t **decompressed_tvb, int *decompressed_offset)
{
    switch (codec) {
        case KAFKA_MESSAGE_CODEC_SNAPPY:
            return decompress_snappy(tvb, pinfo, offset, length, decompressed_tvb, decompressed_offset);
        case KAFKA_MESSAGE_CODEC_LZ4:
            return decompress_lz4(tvb, pinfo, offset, length, decompressed_tvb, decompressed_offset);
        case KAFKA_MESSAGE_CODEC_ZSTD:
            return decompress_zstd(tvb, pinfo, offset, length, decompressed_tvb, decompressed_offset);
        case KAFKA_MESSAGE_CODEC_GZIP:
            return decompress_gzip(tvb, pinfo, offset, length, decompressed_tvb, decompressed_offset);
        case KAFKA_MESSAGE_CODEC_NONE:
            return decompress_none(tvb, pinfo, offset, length, decompressed_tvb, decompressed_offset);
        default:
            col_append_str(pinfo->cinfo, COL_INFO, " [unsupported compression type]");
            return 0;
    }
}

/*
 * Function: dissect_kafka_message_old
 * ---------------------------------------------------
 * Handles decoding of pre-0.11 message format. In the old format
 * only the message payload was the subject of compression
 * and the batches were special kind of message payload.
 *
 * https://kafka.apache.org/0100/documentation/#messageformat
 *
 * tvb: actual data buffer
 * pinfo: packet information
 * tree: protocol information tree to append the item
 * hf_item: protocol information item descriptor index
 * offset: pointer to the message
 *
 * returns: pointer to the next message/batch
 */
static int
dissect_kafka_message_old(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
    proto_item  *message_ti;
    proto_tree  *subtree;
    tvbuff_t    *decompressed_tvb;
    int         decompressed_offset;
    int         start_offset = offset;
    gint8       magic_byte;
    guint8      codec;
    guint32     message_size;
    guint32     length;

    message_size = tvb_get_guint32(tvb, start_offset + 8, ENC_BIG_ENDIAN);

    subtree = proto_tree_add_subtree(tree, tvb, start_offset, message_size + 12, ett_kafka_message, &message_ti, "Message");

    proto_tree_add_item(subtree, hf_kafka_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(subtree, hf_kafka_message_size, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_tree_add_item(subtree, hf_kafka_message_crc, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    magic_byte = tvb_get_guint8(tvb, offset);

    proto_tree_add_item(subtree, hf_kafka_message_magic, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_tree_add_item(subtree, hf_kafka_message_codec, tvb, offset, 1, ENC_BIG_ENDIAN);
    codec = tvb_get_guint8(tvb, offset) & KAFKA_MESSAGE_CODEC_MASK;
    proto_tree_add_item(subtree, hf_kafka_message_timestamp_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    if (magic_byte == 1) {
        proto_tree_add_item(subtree, hf_kafka_message_timestamp, tvb, offset, 8, ENC_TIME_MSECS|ENC_BIG_ENDIAN);
        offset += 8;
    }

    offset = dissect_kafka_bytes(subtree, hf_kafka_message_key, tvb, pinfo, offset, NULL, NULL);

    /*
     * depending on the compression codec, the payload is the actual message payload (codes=none)
     * or compressed set of messages (otherwise). In the new format (since Kafka 1.0) there
     * is no such duality.
     */
    if (codec == 0) {
        offset = dissect_kafka_bytes(subtree, hf_kafka_message_value, tvb, pinfo, offset, NULL, &length);
    } else {
        length = tvb_get_ntohl(tvb, offset);
        offset += 4;
        if (decompress(tvb, pinfo, offset, length, codec, &decompressed_tvb, &decompressed_offset)==1) {
            add_new_data_source(pinfo, decompressed_tvb, "Decompressed content");
            show_compression_reduction(tvb, subtree, length, tvb_captured_length(decompressed_tvb));
            dissect_kafka_message_set(decompressed_tvb, pinfo, subtree, decompressed_offset,
                tvb_reported_length_remaining(decompressed_tvb, decompressed_offset), codec);
        } else {
            proto_item_append_text(subtree, " [Cannot decompress records]");
        }
        offset += length;
    }

    proto_item_set_end(message_ti, tvb, offset);

    return offset;
}

/*
 * Function: dissect_kafka_message_new
 * ---------------------------------------------------
 * Handles decoding of the new message format. In the new format
 * there is no difference between compressed and plain batch.
 *
 * https://kafka.apache.org/documentation/#messageformat
 *
 * tvb: actual data buffer
 * pinfo: packet information
 * tree: protocol information tree to append the item
 * hf_item: protocol information item descriptor index
 * offset: pointer to the message
 *
 * returns: pointer to the next message/batch
 */
static int
dissect_kafka_message_new(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
    proto_item *batch_ti;
    proto_tree *subtree;
    int         start_offset = offset;
    gint8       magic_byte;
    guint8      codec;
    guint32     message_size;
    guint32     count, i, length;
    guint64     base_offset, first_timestamp;

    tvbuff_t    *decompressed_tvb;
    int         decompressed_offset;

    message_size = tvb_get_guint32(tvb, start_offset + 8, ENC_BIG_ENDIAN);

    subtree = proto_tree_add_subtree(tree, tvb, start_offset, message_size + 12, ett_kafka_batch, &batch_ti, "Record Batch");

    base_offset = tvb_get_ntoh64(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(subtree, hf_kafka_message_size, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    magic_byte = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_message_magic, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    if (magic_byte != 2) {
        proto_item_append_text(subtree, "[Unknown message magic]");
        expert_add_info_format(pinfo, batch_ti, &ei_kafka_unknown_message_magic,
                               "message magic: %d", magic_byte);
        return start_offset + 8 /*base offset*/ + 4 /*message size*/ + message_size;
    }

    proto_tree_add_item(subtree, hf_kafka_batch_crc, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_tree_add_item(subtree, hf_kafka_batch_codec, tvb, offset, 2, ENC_BIG_ENDIAN);
    codec = tvb_get_ntohs(tvb, offset) & KAFKA_MESSAGE_CODEC_MASK;
    proto_tree_add_item(subtree, hf_kafka_batch_timestamp_type, tvb, offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(subtree, hf_kafka_batch_transactional, tvb, offset, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(subtree, hf_kafka_batch_control_batch, tvb, offset, 2, ENC_BIG_ENDIAN);
    // next octet is reserved
    offset += 2;

    proto_tree_add_item(subtree, hf_kafka_batch_last_offset_delta, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    first_timestamp = tvb_get_ntoh64(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_batch_first_timestamp, tvb, offset, 8, ENC_TIME_MSECS|ENC_BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(subtree, hf_kafka_batch_last_timestamp, tvb, offset, 8, ENC_TIME_MSECS|ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(subtree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(subtree, hf_kafka_producer_epoch, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;
    proto_tree_add_item(subtree, hf_kafka_batch_base_sequence, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_tree_add_item(subtree, hf_kafka_batch_size, tvb, offset, 4, ENC_BIG_ENDIAN);
    count = tvb_get_ntohl(tvb, offset);
    offset += 4;

    length = start_offset + 8 /*base offset*/ + 4 /*message size*/ + message_size - offset;

    if (decompress(tvb, pinfo, offset, length, codec, &decompressed_tvb, &decompressed_offset)==1) {
        if (codec != 0) {
            add_new_data_source(pinfo, decompressed_tvb, "Decompressed Records");
            show_compression_reduction(tvb, subtree, length, tvb_captured_length(decompressed_tvb));
        }
        for (i=0;i<count;i++) {
            decompressed_offset = dissect_kafka_record(decompressed_tvb, pinfo, subtree, decompressed_offset, base_offset, first_timestamp);
        }
    } else {
        proto_item_append_text(subtree, " [Cannot decompress records]");
    }

    return start_offset + 8 /*base offset*/ + 4 /*message size*/ + message_size;
}

static int
dissect_kafka_message(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
    gint8       magic_byte;

    magic_byte = tvb_get_guint8(tvb, offset+16);
    if (magic_byte < 2) {
        return dissect_kafka_message_old(tvb, pinfo, tree, offset);
    } else {
        return dissect_kafka_message_new(tvb, pinfo, tree, offset);
    }
}

static int
dissect_kafka_message_set(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, gint offset, guint len, guint8 codec)
{
    proto_item *ti;
    proto_tree *subtree;
    gint        end_offset = offset + len;
    guint       messages = 0;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_message_set, &ti, "Message Set");
    /* If set came from a compressed message, make it obvious in tree root */
    if (codec != KAFKA_MESSAGE_CODEC_NONE) {
        proto_item_append_text(subtree, " [from compressed %s message]", val_to_str_const(codec, kafka_message_codecs, "Unknown"));
    }

    while (offset < end_offset) {
        offset = dissect_kafka_message(tvb, pinfo, subtree, offset);
        messages += 1;
    }

    if (offset != end_offset) {
        expert_add_info(pinfo, ti, &ei_kafka_bad_message_set_length);
    }

    proto_item_append_text(ti, " (%d Messages)", messages);
    proto_item_set_end(ti, tvb, offset);

    return offset;
}

/* OFFSET FETCH REQUEST/RESPONSE */

static int
dissect_kafka_partition_id_ret(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset,
                           kafka_partition_t *p_partition)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    if (p_partition != NULL) {
        *p_partition = tvb_get_ntohl(tvb, offset);
    }
    offset += 4;

    return offset;
}

static int
dissect_kafka_partition_id(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                           kafka_api_version_t api_version _U_)
{
    return dissect_kafka_partition_id_ret(tvb, pinfo, tree, offset, NULL);
}

static int
dissect_kafka_offset_ret(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset,
                     kafka_offset_t *p_offset)
{
    proto_tree_add_item(tree, hf_kafka_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    if (p_offset != NULL) {
        *p_offset = tvb_get_ntoh64(tvb, offset);
    }
    offset += 8;

    return offset;
}

static int
dissect_kafka_offset(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                     kafka_api_version_t api_version _U_)
{
    return dissect_kafka_offset_ret(tvb, pinfo, tree, offset, NULL);
}

static int
dissect_kafka_leader_epoch(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset,
                     kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_offset_time(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset,
                          kafka_api_version_t api_version _U_)
{
    proto_item *ti;
    gint64 message_offset_time;

    message_offset_time = tvb_get_ntoh64(tvb, offset);

    ti = proto_tree_add_item(tree, hf_kafka_offset_time, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    // The query for offset at given time takes the time in milliseconds since epoch.
    // It has two additional special values:
    // * -1 - the latest offset (to consume new messages only)
    // * -2 - the oldest offset (to consume all available messages)
    if (message_offset_time == -1) {
        proto_item_append_text(ti, " (latest)");
    } else if (message_offset_time == -2) {
        proto_item_append_text(ti, " (earliest)");
    }

    return offset;
}

static int
dissect_kafka_offset_fetch_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                         kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    gint32     count;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    count = tvb_get_ntohil(tvb, offset);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version, &dissect_kafka_partition_id);

    proto_item_set_len(ti, offset - start_offset);
    proto_item_append_text(ti, " (Topic: %s, Partitions: %u)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb, topic_start, topic_len, ENC_UTF_8|ENC_NA),
                           count);

    return offset;
}

static int
dissect_kafka_offset_fetch_request_topics(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                         kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    gint32     count;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topics, &ti, "Topics");

    count = tvb_get_ntohil(tvb, offset);

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_fetch_request_topic);

    proto_item_set_len(ti, offset - start_offset);

    if (count < 0) {
        proto_item_append_text(ti, " (all committed topics)");
    } else {
        proto_item_append_text(ti, " (%u topics)", count);
    }

    return offset;
}

static int
dissect_kafka_offset_fetch_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                   kafka_api_version_t api_version)
{
    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_offset_fetch_request_topics(tvb, pinfo, tree, offset, api_version);

    return offset;
}

static int
dissect_kafka_error_ret(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                        kafka_error_t *ret)
{
    kafka_error_t error = (kafka_error_t) tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(tree, hf_kafka_error, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    /* Show error in Info column */
    if (error != 0) {
        col_append_fstr(pinfo->cinfo, COL_INFO,
                        " [%s] ", kafka_error_to_str(error));
    }

    if (ret) {
        *ret = error;
    }

    return offset;
}

static int
dissect_kafka_error(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset)
{
    return dissect_kafka_error_ret(tvb, pinfo, tree, offset, NULL);
}

static int
dissect_kafka_throttle_time(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset)
{
    proto_tree_add_item(tree, hf_kafka_throttle_time, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;
    return offset;
}

static int
dissect_kafka_offset_fetch_response_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                              int start_offset, kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    kafka_packet_values_t packet_values;
    memset(&packet_values, 0, sizeof(packet_values));

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &ti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &packet_values.partition_id);
    offset = dissect_kafka_offset_ret(tvb, pinfo, subtree, offset, &packet_values.offset);

    if (api_version >= 5) {
        offset = dissect_kafka_leader_epoch(tvb, pinfo, subtree, offset, api_version);
    }

    offset = dissect_kafka_string(subtree, hf_kafka_metadata, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    if (packet_values.offset==-1) {
        proto_item_append_text(ti, " (ID=%u, Offset=None)",
                               packet_values.partition_id);
    } else {
        proto_item_append_text(ti, " (ID=%u, Offset=%" G_GINT64_MODIFIER "i)",
                               packet_values.partition_id, packet_values.offset);
    }

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_offset_fetch_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                          kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_fetch_response_partition);

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_offset_fetch_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    if (api_version >= 3) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    return dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                               &dissect_kafka_offset_fetch_response_topic);
}

/* METADATA REQUEST/RESPONSE */

static int
dissect_kafka_metadata_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version _U_)
{
    return dissect_kafka_string(tree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);
}

static int
dissect_kafka_metadata_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                               kafka_api_version_t api_version)
{
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                               &dissect_kafka_metadata_request_topic);

    if (api_version >= 4) {
        proto_tree_add_item(tree, hf_kafka_allow_auto_topic_creation, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    if (api_version >= 8) {
        proto_tree_add_item(tree, hf_kafka_include_cluster_authorized_ops, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    if (api_version >= 8) {
        proto_tree_add_item(tree, hf_kafka_include_topic_authorized_ops, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    return offset;
}

static int
dissect_kafka_metadata_broker(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                              kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    guint32     nodeid;
    int         host_start, host_len;
    guint32     broker_port;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_broker, &ti, "Broker");

    nodeid = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_nodeid, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    offset = dissect_kafka_string(subtree, hf_kafka_broker_host, tvb, pinfo, offset, &host_start, &host_len);

    broker_port = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_port, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 1) {
        offset = dissect_kafka_string(subtree, hf_kafka_rack, tvb, pinfo, offset, NULL, NULL);
    }

    proto_item_append_text(ti, " (node %u: %s:%u)",
                           nodeid,
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                           host_start, host_len, ENC_UTF_8|ENC_NA),
                           broker_port);

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_metadata_replica(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset,
                               kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
    return offset + 4;
}

static int
dissect_kafka_metadata_isr(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset,
                           kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_isr, tvb, offset, 4, ENC_BIG_ENDIAN);
    return offset + 4;
}

static int
dissect_kafka_metadata_offline(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset,
                           kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_offline, tvb, offset, 4, ENC_BIG_ENDIAN);
    return offset + 4;
}

static int
dissect_kafka_metadata_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                 kafka_api_version_t api_version)
{
    proto_item *ti, *subti;
    proto_tree *subtree, *subsubtree;
    int         offset = start_offset;
    int         sub_start_offset;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &ti, "Partition");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &partition);

    proto_tree_add_item(subtree, hf_kafka_leader_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 7) {
        proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    sub_start_offset = offset;
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_replicas, &subti, "Replicas");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version, &dissect_kafka_metadata_replica);
    proto_item_set_len(subti, offset - sub_start_offset);

    sub_start_offset = offset;
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_isrs, &subti, "Caught-Up Replicas");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version, &dissect_kafka_metadata_isr);
    proto_item_set_len(subti, offset - sub_start_offset);

    if (api_version >= 5) {
        sub_start_offset = offset;
        subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_offline, &subti, "Offline Replicas");
        offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version, &dissect_kafka_metadata_offline);
        proto_item_set_len(subti, offset - sub_start_offset);
    }

    proto_item_set_len(ti, offset - start_offset);
    proto_item_append_text(ti, " (ID=%u)", partition);

    return offset;
}

static int
dissect_kafka_metadata_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                             kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    int         name_start, name_length;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &name_start, &name_length);
    proto_item_append_text(ti, " (%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                           name_start, name_length, ENC_UTF_8|ENC_NA));

    if (api_version >= 1) {
        proto_tree_add_item(subtree, hf_kafka_is_internal, tvb, offset, 1, ENC_NA);
        offset += 1;
    }

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version, &dissect_kafka_metadata_partition);

    if (api_version >= 8) {
        proto_tree_add_item(subtree, hf_kafka_topic_authorized_ops, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_metadata_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_brokers, &ti, "Broker Metadata");

    if (api_version >= 3) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version, &dissect_kafka_metadata_broker);
    proto_item_set_len(ti, offset - start_offset);

    if (api_version >= 2) {
        offset = dissect_kafka_string(tree, hf_kafka_cluster_id, tvb, pinfo, offset, NULL, NULL);
    }

    if (api_version >= 1) {
        proto_tree_add_item(tree, hf_kafka_controller_id, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    start_offset = offset;
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topics, &ti, "Topic Metadata");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version, &dissect_kafka_metadata_topic);
    proto_item_set_len(ti, offset - start_offset);

    if (api_version >= 8) {
        proto_tree_add_item(tree, hf_kafka_cluster_authorized_ops, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    return offset;
}

/* LEADER_AND_ISR REQUEST/RESPONSE */

static int
dissect_kafka_leader_and_isr_request_isr(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                         int offset, kafka_api_version_t api_version _U_)
{
    /* isr */
    proto_tree_add_item(tree, hf_kafka_isr, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_leader_and_isr_request_replica(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                             int offset, kafka_api_version_t api_version _U_)
{
    /* replica */
    proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}


static int
dissect_kafka_leader_and_isr_request_partition_state(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                     int offset, kafka_api_version_t api_version)
{
    proto_tree *subtree, *subsubtree;
    proto_item *subti, *subsubti;
    int topic_start, topic_len;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_partition,
                                     &subti, "Partition");

    if (api_version < 2) {
        /* topic */
        offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset,
                                      &topic_start, &topic_len);
    }

    /* partition */
    partition = (kafka_partition_t) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* controller_epoch */
    proto_tree_add_item(subtree, hf_kafka_controller_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* leader */
    proto_tree_add_item(subtree, hf_kafka_leader_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* leader_epoch */
    proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* [isr] */
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_isrs,
                                        &subsubti, "ISRs");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_leader_and_isr_request_isr);
    proto_item_set_end(subsubti, tvb, offset);

    /* zk_version */
    proto_tree_add_item(subtree, hf_kafka_zk_version, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* [replica] */
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_replicas,
                                        &subsubti, "Current Replicas");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_leader_and_isr_request_replica);
    proto_item_set_end(subsubti, tvb, offset);

    if (api_version >= 3) {

        subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                            ett_kafka_replicas,
                                            &subsubti, "Adding Replicas");
        offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_leader_and_isr_request_replica);
        proto_item_set_end(subsubti, tvb, offset);

        subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                            ett_kafka_replicas,
                                            &subsubti, "Removing Replicas");
        offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_leader_and_isr_request_replica);
        proto_item_set_end(subsubti, tvb, offset);

    }

    proto_item_set_end(subti, tvb, offset);

    if (api_version < 2) {
        proto_item_append_text(subti, " (Topic=%s, Partition-ID=%u)",
                               tvb_get_string_enc(wmem_packet_scope(), tvb,
                                                  topic_start, topic_len, ENC_UTF_8|ENC_NA),
                               partition);
    } else {
        proto_item_append_text(subti, " (Partition-ID=%u)",
                               partition);
    }

    return offset;
}

static int
dissect_kafka_leader_and_isr_request_topic_state(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                     int offset, kafka_api_version_t api_version)
{
    proto_tree *subtree;
    proto_item *subti;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topic,
                                     &subti, "Topic");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset,
                                  &topic_start, &topic_len);

    /* [partition_state] */
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_leader_and_isr_request_partition_state);

    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_leader_and_isr_request_live_leader(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                 int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    gint32 nodeid;
    int host_start, host_len;
    gint32 broker_port;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_broker,
                                     &subti, "Live Leader");

    /* id */
    nodeid = (kafka_partition_t) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_nodeid, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* host */
    offset = dissect_kafka_string(subtree, hf_kafka_broker_host, tvb, pinfo, offset, &host_start, &host_len);

    /* port */
    broker_port = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_port, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (node %u: %s:%u)",
                           nodeid,
                           tvb_get_string_enc(wmem_packet_scope(), tvb, host_start, host_len, ENC_UTF_8|ENC_NA),
                           broker_port);

    return offset;
}

static int
dissect_kafka_leader_and_isr_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    gint32 controller_id;

    /* controller_id */
    controller_id = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(tree, hf_kafka_controller_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* controller_epoch */
    proto_tree_add_item(tree, hf_kafka_controller_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 2) {
        /* broker_epoch */
        proto_tree_add_item(tree, hf_kafka_broker_epoch, tvb, offset, 8, ENC_BIG_ENDIAN);
        offset += 8;
    }

    if (api_version >= 2) {
        /* [topic_state] */
        offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_leader_and_isr_request_topic_state);
    } else {
        /* [partition_state] */
        offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_leader_and_isr_request_partition_state);
    }

    /* [live_leader] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_leader_and_isr_request_live_leader);

    col_append_fstr(pinfo->cinfo, COL_INFO, " (Controller-ID=%d)", controller_id);

    return offset;
}

static int
dissect_kafka_leader_and_isr_response_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;
    kafka_partition_t partition;
    kafka_error_t error;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_partition,
                                     &subti, "Partition");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    /* partition */
    partition = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* error_code */
    offset = dissect_kafka_error_ret(tvb, pinfo, subtree, offset, &error);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s, Partition-ID=%u, Error=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA),
                           partition,
                           kafka_error_to_str(error));

    return offset;
}

static int
dissect_kafka_leader_and_isr_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                      kafka_api_version_t api_version)
{
    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    /* [partition] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_leader_and_isr_response_partition);

    return offset;
}

/* STOP_REPLICA REQUEST/RESPONSE */

static int
dissect_kafka_stop_replica_request_topic_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                             int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;
    return offset;
}

static int
dissect_kafka_stop_replica_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                             int offset, kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topic,
                                     &subti, "Topic");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_stop_replica_request_topic_partition);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_stop_replica_request_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                             int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_partition,
                                     &subti, "Partition");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    /* partition */
    partition = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s, Partition-ID=%u)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA),
                           partition);

    return offset;
}

static int
dissect_kafka_stop_replica_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                   kafka_api_version_t api_version)
{
    gint32 controller_id;

    /* controller_id */
    controller_id = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(tree, hf_kafka_controller_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* controller_epoch */
    proto_tree_add_item(tree, hf_kafka_controller_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 1) {
        /* broker_epoch */
        proto_tree_add_item(tree, hf_kafka_broker_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    /* delete_partitions */
    proto_tree_add_item(tree, hf_kafka_delete_partitions, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    /* [partition] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_stop_replica_request_partition);

    if (api_version >= 1) {
        offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_stop_replica_request_topic);
    }

    col_append_fstr(pinfo->cinfo, COL_INFO, " (Controller-ID=%d)", controller_id);

    return offset;
}

static int
dissect_kafka_stop_replica_response_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;
    kafka_error_t error;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_partition,
                                     &subti, "Partition");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    /* partition */
    partition = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* error_code */
    offset = dissect_kafka_error_ret(tvb, pinfo, subtree, offset, &error);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s, Partition-ID=%u, Error=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA),
                           partition,
                           kafka_error_to_str(error));

    return offset;
}

static int
dissect_kafka_stop_replica_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    /* [partition] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_stop_replica_response_partition);

    return offset;
}

/* FETCH REQUEST/RESPONSE */

static int
dissect_kafka_fetch_request_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                      kafka_api_version_t api_version _U_)
{
    proto_item *ti;
    proto_tree *subtree;
    kafka_packet_values_t packet_values;
    memset(&packet_values, 0, sizeof(packet_values));

    subtree = proto_tree_add_subtree(tree, tvb, offset, 16, ett_kafka_partition, &ti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &packet_values.partition_id);

    if (api_version >= 9) {
        proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    offset = dissect_kafka_offset_ret(tvb, pinfo, subtree, offset, &packet_values.offset);

    if (api_version >= 5) {
        proto_tree_add_item(subtree, hf_kafka_log_start_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
        offset += 8;
    }

    proto_tree_add_item(subtree, hf_kafka_max_bytes, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_item_append_text(ti, " (ID=%u, Offset=%" G_GINT64_MODIFIER "i)",
                           packet_values.partition_id, packet_values.offset);

    return offset;
}

static int
dissect_kafka_fetch_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                  kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    guint32     count;
    int         name_start, name_length;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &name_start, &name_length);
    count = tvb_get_ntohl(tvb, offset);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_fetch_request_partition);

    proto_item_set_len(ti, offset - start_offset);
    proto_item_append_text(ti, " (%u partitions)", count);

    return offset;
}

static int
dissect_kafka_fetch_request_forgottent_topic_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree, int offset,
                                                  kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_forgotten_topic_partition, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;
    return offset;
}

static int
dissect_kafka_fetch_request_forgotten_topics_data(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                  kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    guint32     count;
    int         name_start, name_length;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_request_forgotten_topic, &ti, "Fetch Request Forgotten Topic Data");

    offset = dissect_kafka_string(subtree, hf_kafka_forgotten_topic_name, tvb, pinfo, offset, &name_start, &name_length);
    count = tvb_get_ntohl(tvb, offset);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_fetch_request_forgottent_topic_partition);

    proto_item_set_len(ti, offset - start_offset);
    proto_item_append_text(ti, " (%u partitions)", count);

    return offset;
}

static int
dissect_kafka_fetch_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                            kafka_api_version_t api_version)
{
    proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_tree_add_item(tree, hf_kafka_max_wait_time, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_tree_add_item(tree, hf_kafka_min_bytes, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 3) {
        proto_tree_add_item(tree, hf_kafka_max_bytes, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    if (api_version >= 4) {
        proto_tree_add_item(tree, hf_kafka_isolation_level, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    if (api_version >= 7) {
        proto_tree_add_item(tree, hf_kafka_fetch_session_id, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    if (api_version >= 7) {
        proto_tree_add_item(tree, hf_kafka_fetch_session_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version, &dissect_kafka_fetch_request_topic);

    if (api_version >= 7) {
        offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version, &dissect_kafka_fetch_request_forgotten_topics_data);
    }

    if (api_version >= 11) {
        offset = dissect_kafka_string(tree, hf_kafka_rack, tvb, pinfo, offset, NULL, NULL);
    }

    return offset;
}

static int
dissect_kafka_aborted_transaction(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                  int start_offset, kafka_api_version_t api_version _U_)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_aborted_transaction, &ti, "Transaction");

    proto_tree_add_item(subtree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(subtree, hf_kafka_first_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_aborted_transactions(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                  int start_offset, kafka_api_version_t api_version _U_)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_aborted_transactions, &ti, "Aborted Transactions");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version, &dissect_kafka_aborted_transaction);

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_fetch_response_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                       kafka_api_version_t api_version _U_)
{
    proto_item *ti;
    proto_tree *subtree;
    int        offset = start_offset;
    guint      len;
    kafka_packet_values_t packet_values;
    memset(&packet_values, 0, sizeof(packet_values));

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &ti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &packet_values.partition_id);

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_offset_ret(tvb, pinfo, subtree, offset, &packet_values.offset);

    if (api_version >= 4) {
        proto_tree_add_item(subtree, hf_kafka_last_stable_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
        offset += 8;
    }

    if (api_version >= 5) {
        proto_tree_add_item(subtree, hf_kafka_log_start_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
        offset += 8;
    }

    if (api_version >= 4) {
        offset = dissect_kafka_aborted_transactions(tvb, pinfo, subtree, offset, api_version);
    }

    len = tvb_get_ntohl(tvb, offset);
    offset += 4;

    if (len > 0) {
        offset = dissect_kafka_message_set(tvb, pinfo, subtree, offset, len, KAFKA_MESSAGE_CODEC_NONE);
    }

    proto_item_set_len(ti, offset - start_offset);

    proto_item_append_text(ti, " (ID=%u, Offset=%" G_GINT64_MODIFIER "i)",
                           packet_values.partition_id, packet_values.offset);

    return offset;
}

static int
dissect_kafka_fetch_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                   kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    guint32     count;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);
    count = tvb_get_ntohl(tvb, offset);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_fetch_response_partition);

    proto_item_set_len(ti, offset - start_offset);
    proto_item_append_text(ti, " (%u partitions)", count);

    return offset;
}

static int
dissect_kafka_fetch_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                             kafka_api_version_t api_version)
{
    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    if (api_version >= 7) {
        offset = dissect_kafka_error(tvb, pinfo, tree, offset);
    }

    if (api_version >= 7) {
        proto_tree_add_item(tree, hf_kafka_fetch_session_id, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    if (api_version >= 11) {
        proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    return dissect_kafka_array(tree, tvb, pinfo, offset, api_version, &dissect_kafka_fetch_response_topic);
}

/* PRODUCE REQUEST/RESPONSE */

static int
dissect_kafka_produce_request_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version _U_)
{
    proto_item *ti;
    proto_tree *subtree;
    guint      len;
    kafka_packet_values_t packet_values;
    memset(&packet_values, 0, sizeof(packet_values));

    subtree = proto_tree_add_subtree(tree, tvb, offset, 14, ett_kafka_partition, &ti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &packet_values.partition_id);

    len = tvb_get_ntohl(tvb, offset);
    offset += 4;

    if (len > 0) {
        offset = dissect_kafka_message_set(tvb, pinfo, subtree, offset, len, KAFKA_MESSAGE_CODEC_NONE);
    }

    proto_item_append_text(ti, " (ID=%u)", packet_values.partition_id);
    proto_item_set_end(ti, tvb, offset);

    return offset;
}

static int
dissect_kafka_produce_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                    kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    int topic_off, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_off, &topic_len);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_produce_request_partition);

    proto_item_append_text(ti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb, topic_off, topic_len, ENC_UTF_8|ENC_NA));
    proto_item_set_end(ti, tvb, offset);

    return offset;
}

static int
dissect_kafka_produce_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                              kafka_api_version_t api_version)
{
    if (api_version >= 3) {
        offset = dissect_kafka_string(tree, hf_kafka_transactional_id, tvb, pinfo, offset, NULL, NULL);
    }

    proto_tree_add_item(tree, hf_kafka_required_acks, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_tree_add_item(tree, hf_kafka_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_produce_request_topic);

    return offset;
}

static int
dissect_kafka_produce_response_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                         kafka_api_version_t api_version _U_)
{
    proto_item *ti;
    proto_tree *subtree;
    kafka_packet_values_t packet_values;
    memset(&packet_values, 0, sizeof(packet_values));

    subtree = proto_tree_add_subtree(tree, tvb, offset, 14, ett_kafka_partition, &ti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &packet_values.partition_id);

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_offset_ret(tvb, pinfo, subtree, offset, &packet_values.offset);

    if (api_version >= 2) {
        offset = dissect_kafka_offset_time(tvb, pinfo, subtree, offset, api_version);
    }

    if (api_version >= 5) {
        proto_tree_add_item(subtree, hf_kafka_log_start_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
        offset += 8;
    }

    proto_item_append_text(ti, " (ID=%u, Offset=%" G_GINT64_MODIFIER "i)",
                           packet_values.partition_id, packet_values.offset);

    return offset;
}

static int
dissect_kafka_produce_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                     kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_produce_response_partition);

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_produce_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                               kafka_api_version_t api_version)
{
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version, &dissect_kafka_produce_response_topic);

    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    return offset;
}

/* OFFSETS REQUEST/RESPONSE */

static int
dissect_kafka_offsets_request_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                        int start_offset, kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &ti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &partition);

    if (api_version >= 4) {
        proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    offset = dissect_kafka_offset_time(tvb, pinfo, subtree, offset, api_version);

    if (api_version == 0) {
        proto_tree_add_item(subtree, hf_kafka_max_offsets, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    proto_item_set_len(ti, offset - start_offset);
    proto_item_append_text(ti, " (ID=%u)", partition);

    return offset;
}

static int
dissect_kafka_offsets_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                    kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offsets_request_partition);

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_offsets_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                              kafka_api_version_t api_version)
{
    proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 2) {
        proto_tree_add_item(tree, hf_kafka_isolation_level, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version, &dissect_kafka_offsets_request_topic);

    return offset;
}

static int
dissect_kafka_offsets_response_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                         int start_offset, kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &ti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &partition);

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    if (api_version == 0) {
        offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version, &dissect_kafka_offset);
    }
    else if (api_version >= 1) {
        offset = dissect_kafka_offset_time(tvb, pinfo, subtree, offset, api_version);

        offset = dissect_kafka_offset_ret(tvb, pinfo, subtree, offset, NULL);
    }

    if (api_version >= 4) {
        proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    proto_item_set_len(ti, offset - start_offset);
    proto_item_append_text(ti, " (ID=%u)", partition);

    return offset;
}

static int
dissect_kafka_offsets_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                                     kafka_api_version_t api_version)
{
    proto_item *ti;
    proto_tree *subtree;
    int         offset = start_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &ti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offsets_response_partition);

    proto_item_set_len(ti, offset - start_offset);

    return offset;
}

static int
dissect_kafka_offsets_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int start_offset,
                               kafka_api_version_t api_version)
{
    int offset = start_offset;

    if (api_version >= 2) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    return dissect_kafka_array(tree, tvb, pinfo, offset, api_version, &dissect_kafka_offsets_response_topic);
}

/* API_VERSIONS REQUEST/RESPONSE */

static int
dissect_kafka_api_versions_request(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_,
                                   int offset _U_, kafka_api_version_t api_version _U_)
{
    return offset;
}

static int
dissect_kafka_api_versions_response_api_version(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                int offset, kafka_api_version_t api_version _U_)
{
    proto_item *ti;
    proto_tree *subtree;
    kafka_api_key_t api_key;
    kafka_api_version_t min_version, max_version;
    const kafka_api_info_t *api_info;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_api_version, &ti,
                                     "API Version");

    api_key = tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_api_versions_api_key, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    min_version = tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_api_versions_min_version, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    max_version = tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_api_versions_max_version, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_item_set_end(ti, tvb, offset);
    if (max_version != min_version) {
        /* Range of versions supported. */
        proto_item_append_text(subtree, " %s (v%d-%d)",
                               kafka_api_key_to_str(api_key),
                               min_version, max_version);
    }
    else {
        /* Only one version. */
        proto_item_append_text(subtree, " %s (v%d)",
                               kafka_api_key_to_str(api_key),
                               min_version);
    }

    api_info = kafka_get_api_info(api_key);
    if (api_info == NULL) {
        proto_item_append_text(subtree, " [Unknown API key]");
        expert_add_info_format(pinfo, ti, &ei_kafka_unknown_api_key,
                               "%s API key", kafka_api_key_to_str(api_key));
    }
    else if (!kafka_is_api_version_supported(api_info, min_version) ||
             !kafka_is_api_version_supported(api_info, max_version)) {
        if (api_info->min_version == -1) {
            proto_item_append_text(subtree, " [Unsupported API version]");
            expert_add_info_format(pinfo, ti, &ei_kafka_unsupported_api_version,
                                   "Unsupported %s version.",
                                   kafka_api_key_to_str(api_key));
        }
        else if (api_info->min_version == api_info->max_version) {
            proto_item_append_text(subtree, " [Unsupported API version. Supports v%d]",
                                   api_info->min_version);
            expert_add_info_format(pinfo, ti, &ei_kafka_unsupported_api_version,
                                   "Unsupported %s version. Supports v%d.",
                                   kafka_api_key_to_str(api_key), api_info->min_version);
        } else {
            proto_item_append_text(subtree, " [Unsupported API version. Supports v%d-%d]",
                                   api_info->min_version, api_info->max_version);
            expert_add_info_format(pinfo, ti, &ei_kafka_unsupported_api_version,
                                   "Unsupported %s version. Supports v%d-%d.",
                                   kafka_api_key_to_str(api_key),
                                   api_info->min_version, api_info->max_version);
        }
    }

    return offset;
}

static int
dissect_kafka_api_versions_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    /* [api_version] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_api_versions_response_api_version);

    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }
    return offset;
}

/* UPDATE_METADATA REQUEST/RESPONSE */

static int
dissect_kafka_update_metadata_request_isr(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                          int offset, kafka_api_version_t api_version _U_)
{
    /* isr */
    proto_tree_add_item(tree, hf_kafka_isr, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_update_metadata_request_replica(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version _U_)
{
    /* replica */
    proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_update_metadata_request_partition_state(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version)
{
    proto_tree *subtree, *subsubtree;
    proto_item *subti, *subsubti;
    int topic_start, topic_len;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_partition,
                                     &subti, "Partition");

    if (api_version < 5) {
        /* topic */
        offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset,
                                      &topic_start, &topic_len);
    }

    /* partition */
    partition = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* controller_epoch */
    proto_tree_add_item(subtree, hf_kafka_controller_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* leader */
    proto_tree_add_item(subtree, hf_kafka_leader_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* leader_epoch */
    proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* [isr] */
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_isrs,
                                        &subsubti, "ISRs");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_update_metadata_request_isr);
    proto_item_set_end(subsubti, tvb, offset);

    /* zk_version */
    proto_tree_add_item(subtree, hf_kafka_zk_version, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* [replica] */
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_replicas,
                                        &subsubti, "Replicas");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_update_metadata_request_replica);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);

    if (api_version >= 5) {
        proto_item_append_text(subti, " (Partition-ID=%u)",
                               partition);
    } else {
        proto_item_append_text(subti, " (Topic=%s, Partition-ID=%u)",
                               tvb_get_string_enc(wmem_packet_scope(), tvb,
                                                  topic_start, topic_len, ENC_UTF_8|ENC_NA),
                               partition);
    }
    return offset;
}

static int
dissect_kafka_update_metadata_request_topic_state(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version)
{
    proto_tree *subtree;
    proto_item *subti;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topic,
                                     &subti, "Topic");
    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset,
                                  &topic_start, &topic_len);

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_update_metadata_request_partition_state);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                    topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_update_metadata_request_end_point(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int host_start, host_len;
    gint32 broker_port;
    gint16 security_protocol_type;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_broker_end_point,
                                     &subti, "End Point");

    /* port */
    broker_port = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_port, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* host */
    offset = dissect_kafka_string(subtree, hf_kafka_broker_host, tvb, pinfo, offset, &host_start, &host_len);

    if (api_version >= 3) {
        /* listener_name */
        offset = dissect_kafka_string(subtree, hf_kafka_listener_name, tvb, pinfo, offset, NULL, NULL);
    }

    /* security_protocol_type */
    security_protocol_type = (gint16) tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_security_protocol_type, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (%s://%s:%d)",
                           val_to_str_const(security_protocol_type,
                                            kafka_security_protocol_types, "UNKNOWN"),
                           tvb_get_string_enc(wmem_packet_scope(), tvb, host_start, host_len,
                                              ENC_UTF_8|ENC_NA),
                           broker_port);

    return offset;
}

static int
dissect_kafka_update_metadata_request_live_leader(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                  int offset, kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    gint32 nodeid;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_broker,
                                     &subti, "Live Leader");

    /* id */
    nodeid = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_nodeid, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version == 0) {
        int host_start, host_len;
        gint32 broker_port;

        /* host */
        offset = dissect_kafka_string(subtree, hf_kafka_broker_host, tvb, pinfo, offset, &host_start, &host_len);

        /* port */
        broker_port = (gint32) tvb_get_ntohl(tvb, offset);
        proto_tree_add_item(subtree, hf_kafka_broker_port, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;

        proto_item_append_text(subti, " (node %u: %s:%u)",
                               nodeid,
                               tvb_get_string_enc(wmem_packet_scope(), tvb, host_start, host_len,
                                                  ENC_UTF_8|ENC_NA),
                               broker_port);
    } else if (api_version >= 1) {
        /* [end_point] */
        offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_update_metadata_request_end_point);

        if (api_version >= 2) {
            /* rack */
            offset = dissect_kafka_string(subtree, hf_kafka_rack, tvb, pinfo, offset, NULL, NULL);
        }

        proto_item_append_text(subti, " (node %d)",
                               nodeid);
    }

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_update_metadata_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                      kafka_api_version_t api_version)
{
    gint32 controller_id;

    /* controller_id */
    controller_id = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(tree, hf_kafka_controller_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* controller_epoch */
    proto_tree_add_item(tree, hf_kafka_controller_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 5) {
        /* controller_epoch */
        proto_tree_add_item(tree, hf_kafka_broker_epoch, tvb, offset, 8, ENC_BIG_ENDIAN);
        offset += 8;
    }

    if (api_version >= 5) {
        /* [topic_state] */
        offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_update_metadata_request_topic_state);
    } else {
        /* [partition_state] */
        offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_update_metadata_request_partition_state);
    }

    /* [live_leader] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_update_metadata_request_live_leader);

    col_append_fstr(pinfo->cinfo, COL_INFO, " (Controller-ID=%d)", controller_id);

    return offset;
}

static int
dissect_kafka_update_metadata_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                       kafka_api_version_t api_version _U_)
{
    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    return offset;
}

/* CONTROLLED_SHUTDOWN REQUEST/RESPONSE */

static int
dissect_kafka_controlled_shutdown_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                          kafka_api_version_t api_version _U_)
{
    gint32 broker_id;

    /* broker_id */
    broker_id = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(tree, hf_kafka_broker_nodeid, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 2) {
        proto_tree_add_item(tree, hf_kafka_broker_epoch, tvb, offset, 8, ENC_BIG_ENDIAN);
        offset += 8;
    }

    col_append_fstr(pinfo->cinfo, COL_INFO, " (Broker-ID=%d)", broker_id);

    return offset;
}

static int
dissect_kafka_controlled_shutdown_response_partition_remaining(tvbuff_t *tvb, packet_info *pinfo,
                                                               proto_tree *tree, int offset,
                                                               kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti,
                                     "Partition Remaining");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset,
                                  &topic_start, &topic_len);

    /* partition */
    partition = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s, Partition-ID=%d)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA),
                           partition);

    return offset;
}

static int
dissect_kafka_controlled_shutdown_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                           kafka_api_version_t api_version)
{
    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    /* [partition_remaining] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_controlled_shutdown_response_partition_remaining);

    return offset;
}

/* OFFSET_COMMIT REQUEST/RESPONSE */

static int
dissect_kafka_offset_commit_request_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    kafka_partition_t partition_id;
    kafka_offset_t partition_offset;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti,
                                     "Partition");

    /* partition */
    partition_id = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* offset */
    partition_offset = (gint64) tvb_get_ntoh64(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    if (api_version >= 6) {
        proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    if (api_version == 1) {
        /* timestamp */
        proto_tree_add_item(subtree, hf_kafka_commit_timestamp, tvb, offset, 8, ENC_TIME_MSECS|ENC_BIG_ENDIAN);
        offset += 8;
    }

    /* metadata */
    offset = dissect_kafka_string(subtree, hf_kafka_metadata, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (ID=%u, Offset=%" G_GINT64_MODIFIER "i)",
                           partition_id, partition_offset);

    return offset;
}

static int
dissect_kafka_offset_commit_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                          int offset, kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset,
                                  &topic_start, &topic_len);

    /* [partition] */
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_commit_request_partition);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_offset_commit_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    int group_start, group_len;

    /* group_id */
    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset,
                                  &group_start, &group_len);

    if (api_version >= 1) {
        /* group_generation_id */
        proto_tree_add_item(tree, hf_kafka_generation_id, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;

        /* member_id */
        offset = dissect_kafka_string(tree, hf_kafka_member_id, tvb, pinfo, offset, NULL, NULL);

        if (api_version >= 7){
            /* instance_id */
            offset = dissect_kafka_string(tree, hf_kafka_consumer_group_instance, tvb, pinfo, offset, NULL, NULL);
        }

        if (api_version >= 2 && api_version < 5) {
            /* retention_time */
            proto_tree_add_item(tree, hf_kafka_retention_time, tvb, offset, 8, ENC_BIG_ENDIAN);
            offset += 8;
        }
    }

    /* [topic] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_commit_request_topic);

    col_append_fstr(pinfo->cinfo, COL_INFO,
                    " (Group=%s)",
                    tvb_get_string_enc(wmem_packet_scope(), tvb,
                                       group_start, group_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_offset_commit_response_partition_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                        int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    kafka_partition_t partition;
    kafka_error_t error;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti,
                                     "Partition");

    /* partition */
    partition = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* error_code */
    offset = dissect_kafka_error_ret(tvb, pinfo, subtree, offset, &error);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Partition-ID=%d, Error=%s)",
                           partition, kafka_error_to_str(error));

    return offset;
}

static int
dissect_kafka_offset_commit_response_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset,
                                  &topic_start, &topic_len);

    /* [partition_response] */
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_commit_response_partition_response);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_offset_commit_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    if (api_version >= 3) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* [responses] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_commit_response_response);

    return offset;
}

/* GROUP_COORDINATOR REQUEST/RESPONSE */

static int
dissect_kafka_find_coordinator_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version _U_)
{
    int group_start, group_len;

    if (api_version == 0) {
        /* group_id */
        offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset,
                                      &group_start, &group_len);

        col_append_fstr(pinfo->cinfo, COL_INFO,
                        " (Group=%s)",
                        tvb_get_string_enc(wmem_packet_scope(), tvb,
                                           group_start, group_len, ENC_UTF_8|ENC_NA));
    } else {

        offset = dissect_kafka_string(tree, hf_kafka_coordinator_key, tvb, pinfo, offset,
                                      NULL, NULL);

        proto_tree_add_item(tree, hf_kafka_coordinator_type, tvb, offset, 1, ENC_NA);
        offset += 4;

    }

    return offset;
}

static int
dissect_kafka_find_coordinator_response_coordinator(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                     int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    gint32 node_id;
    int host_start, host_len;
    gint32 port;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_broker, &subti, "Coordinator");

    /* node_id */
    node_id = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_nodeid, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* host */
    offset = dissect_kafka_string(subtree, hf_kafka_broker_host, tvb, pinfo, offset,
                                  &host_start, &host_len);

    /* port */
    port = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_broker_port, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_item_set_end(subti, tvb, offset);

    if (node_id >= 0) {
        proto_item_append_text(subti, " (node %d: %s:%d)",
                               node_id,
                               tvb_get_string_enc(wmem_packet_scope(), tvb,
                                                  host_start, host_len, ENC_UTF_8|ENC_NA),
                               port);
    } else {
        proto_item_append_text(subti, " (none)");
    }

    return offset;
}

static int
dissect_kafka_find_coordinator_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                         kafka_api_version_t api_version)
{
    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    if (api_version >= 1) {
        offset = dissect_kafka_string(tree, hf_kafka_error_message, tvb, pinfo, offset,
                                      NULL, NULL);
    }

    /* coordinator */
    offset = dissect_kafka_find_coordinator_response_coordinator(tvb, pinfo, tree, offset, api_version);

    return offset;
}

/* JOIN_GROUP REQUEST/RESPONSE */

static int
dissect_kafka_join_group_request_group_protocols(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                 int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int protocol_start, protocol_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_protocol, &subti,
                                     "Group Protocol");

    /* protocol_name */
    offset = dissect_kafka_string(subtree, hf_kafka_protocol_name, tvb, pinfo, offset,
                                  &protocol_start, &protocol_len);

    /* protocol_metadata */
    offset = dissect_kafka_bytes(subtree, hf_kafka_protocol_metadata, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Group-ID=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              protocol_start, protocol_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_join_group_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                 kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    int group_start, group_len;
    int member_start, member_len;

    /* group_id */
    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset,
                                  &group_start, &group_len);

    /* session_timeout */
    proto_tree_add_item(tree, hf_kafka_session_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version > 0) {
        /* rebalance_timeout */
        proto_tree_add_item(tree, hf_kafka_rebalance_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    /* member_id */
    offset = dissect_kafka_string(tree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    if (api_version >= 5) {
        /* instance id */
        offset = dissect_kafka_string(tree, hf_kafka_consumer_group_instance, tvb, pinfo, offset,
                                      NULL, NULL);
    }

    /* protocol_type */
    offset = dissect_kafka_string(tree, hf_kafka_protocol_type, tvb, pinfo, offset, NULL, NULL);

    /* [group_protocols] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_protocols, &subti,
                                     "Group Protocols");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_join_group_request_group_protocols);
    proto_item_set_end(subti, tvb, offset);

    col_append_fstr(pinfo->cinfo, COL_INFO,
                    " (Group=%s, Member=%s)",
                    tvb_get_string_enc(wmem_packet_scope(), tvb,
                                       group_start, group_len, ENC_UTF_8|ENC_NA),
                    tvb_get_string_enc(wmem_packet_scope(), tvb,
                                       member_start, member_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_join_group_response_member(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                         int offset, kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    int member_start, member_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_member, &subti, "Member");

    /* member_id */
    offset = dissect_kafka_string(subtree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    if (api_version >= 5) {
        /* instance id */
        offset = dissect_kafka_string(subtree, hf_kafka_consumer_group_instance, tvb, pinfo, offset,
                                      NULL, NULL);
    }

    /* member_metadata */
    offset = dissect_kafka_bytes(subtree, hf_kafka_member_metadata, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Member=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              member_start, member_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_join_group_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                  kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    int member_start, member_len;

    if (api_version >= 2) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    /* generation_id */
    proto_tree_add_item(tree, hf_kafka_generation_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* group_protocol */
    offset = dissect_kafka_string(tree, hf_kafka_protocol_name, tvb, pinfo, offset, NULL, NULL);

    /* leader_id */
    offset = dissect_kafka_string(tree, hf_kafka_group_leader_id, tvb, pinfo, offset, NULL, NULL);

    /* member_id */
    offset = dissect_kafka_string(tree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    /* [member] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_members, &subti, "Members");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_join_group_response_member);
    proto_item_set_end(subti, tvb, offset);

    col_append_fstr(pinfo->cinfo, COL_INFO,
                    " (Member=%s)",
                    tvb_get_string_enc(wmem_packet_scope(), tvb,
                                       member_start, member_len, ENC_UTF_8|ENC_NA));

    return offset;
}

/* HEARTBEAT REQUEST/RESPONSE */

static int
dissect_kafka_heartbeat_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                kafka_api_version_t api_version _U_)
{
    int group_start, group_len;
    int member_start, member_len;

    /* group_id */
    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset,
                                  &group_start, &group_len);

    /* group_generation_id */
    proto_tree_add_item(tree, hf_kafka_generation_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* member_id */
    offset = dissect_kafka_string(tree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    if (api_version >= 3) {
        /* instance_id */
        offset = dissect_kafka_string(tree, hf_kafka_consumer_group_instance, tvb, pinfo, offset,
                                      NULL, NULL);
    }

    col_append_fstr(pinfo->cinfo, COL_INFO,
                    " (Group=%s, Member=%s)",
                    tvb_get_string_enc(wmem_packet_scope(), tvb,
                                       group_start, group_len, ENC_UTF_8|ENC_NA),
                    tvb_get_string_enc(wmem_packet_scope(), tvb,
                                       member_start, member_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_heartbeat_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                 kafka_api_version_t api_version _U_)
{
    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    return offset;
}

/* LEAVE_GROUP REQUEST/RESPONSE */

static int
dissect_kafka_leave_group_request_member(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                  kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int member_start, member_len;
    int instance_start, instance_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_member, &subti, "Member");

    /* member_id */
    offset = dissect_kafka_string(subtree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    /* instance_id */
    offset = dissect_kafka_string(subtree, hf_kafka_consumer_group_instance, tvb, pinfo, offset,
                                  &instance_start, &instance_len);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Member=%s, Group-Instance=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              member_start, member_len, ENC_UTF_8|ENC_NA),
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              instance_start, instance_len, ENC_UTF_8|ENC_NA)
                           );

    return offset;
}

static int
dissect_kafka_leave_group_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                  kafka_api_version_t api_version)
{
    int group_start, group_len;
    int member_start, member_len;
    proto_item *subti;
    proto_tree *subtree;

    /* group_id */
    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset,
                                  &group_start, &group_len);

    if (api_version >= 0 && api_version <= 2) {

        /* member_id */
        offset = dissect_kafka_string(tree, hf_kafka_member_id, tvb, pinfo, offset,
                                      &member_start, &member_len);

        col_append_fstr(pinfo->cinfo, COL_INFO,
                        " (Group=%s, Member=%s)",
                        tvb_get_string_enc(wmem_packet_scope(), tvb,
                                           group_start, group_len, ENC_UTF_8|ENC_NA),
                        tvb_get_string_enc(wmem_packet_scope(), tvb,
                                           member_start, member_len, ENC_UTF_8|ENC_NA));

    } else if (api_version >= 3) {

        // KIP-345
        subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_members, &subti, "Members");
        offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_leave_group_request_member);
        proto_item_set_end(subti, tvb, offset);

        col_append_fstr(pinfo->cinfo, COL_INFO,
                        " (Group=%s)",
                        tvb_get_string_enc(wmem_packet_scope(), tvb,
                                           group_start, group_len, ENC_UTF_8|ENC_NA));

    }

    return offset;
}

static int
dissect_kafka_leave_group_response_member(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                                  kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int member_start, member_len;
    int instance_start, instance_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_member, &subti, "Member");

    /* member_id */
    offset = dissect_kafka_string(subtree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    /* instance_id */
    offset = dissect_kafka_string(subtree, hf_kafka_consumer_group_instance, tvb, pinfo, offset,
                                  &instance_start, &instance_len);

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Member=%s, Group-Instance=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              member_start, member_len, ENC_UTF_8|ENC_NA),
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              instance_start, instance_len, ENC_UTF_8|ENC_NA)
                           );

    return offset;
}

static int
dissect_kafka_leave_group_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                   kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    if (api_version >= 3) {

        // KIP-345
        subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_members, &subti, "Members");
        offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_leave_group_response_member);
        proto_item_set_end(subti, tvb, offset);

    }


    return offset;
}

/* SYNC_GROUP REQUEST/RESPONSE */

static int
dissect_kafka_sync_group_request_group_assignment(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                  int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int member_start, member_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_assignment, &subti,
                                     "Group Assignment");

    /* member_id */
    offset = dissect_kafka_string(subtree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    /* member_assigment */
    offset = dissect_kafka_bytes(subtree, hf_kafka_member_assignment, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Member=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              member_start, member_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_sync_group_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                 kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    int group_start, group_len;
    int member_start, member_len;

    /* group_id */
    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset,
                                  &group_start, &group_len);

    /* generation_id */
    proto_tree_add_item(tree, hf_kafka_generation_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* member_id */
    offset = dissect_kafka_string(tree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    if (api_version >= 3) {
        /* instance_id */
        offset = dissect_kafka_string(tree, hf_kafka_consumer_group_instance, tvb, pinfo, offset,
                                  NULL, NULL);
    }

    /* [group_assignment] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_assignments, &subti,
                                     "Group Assignments");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_sync_group_request_group_assignment);
    proto_item_set_end(subti, tvb, offset);

    col_append_fstr(pinfo->cinfo, COL_INFO,
                    " (Group=%s, Member=%s)",
                    tvb_get_string_enc(wmem_packet_scope(), tvb,
                                       group_start, group_len, ENC_UTF_8|ENC_NA),
                    tvb_get_string_enc(wmem_packet_scope(), tvb,
                                       member_start, member_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_sync_group_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                  kafka_api_version_t api_version _U_)
{
    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    /* member_assignment */
    offset = dissect_kafka_bytes(tree, hf_kafka_member_assignment, tvb, pinfo, offset, NULL, NULL);

    return offset;
}

/* DESCRIBE_GROUPS REQUEST/RESPONSE */

static int
dissect_kafka_describe_groups_request_group_id(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version _U_)
{
    /* group_id */
    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset, NULL, NULL);

    return offset;
}

static int
dissect_kafka_describe_groups_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                      kafka_api_version_t api_version)
{
    /* [group_id] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_groups_request_group_id);

    if (api_version >= 3) {
        proto_tree_add_item(tree, hf_kafka_include_group_authorized_ops, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    return offset;
}

static int
dissect_kafka_describe_groups_response_member(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                              kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int member_start, member_len;
    int instance_start, instance_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group_member, &subti, "Member");

    /* member_id */
    offset = dissect_kafka_string(subtree, hf_kafka_member_id, tvb, pinfo, offset,
                                  &member_start, &member_len);

    if (api_version >= 4) {
        /* instance_id */
        offset = dissect_kafka_string(subtree, hf_kafka_consumer_group_instance, tvb, pinfo, offset,
                                      &instance_start, &instance_len);
    }

    /* client_id */
    offset = dissect_kafka_string(subtree, hf_kafka_client_id, tvb, pinfo, offset, NULL, NULL);

    /* client_host */
    offset = dissect_kafka_string(subtree, hf_kafka_client_host, tvb, pinfo, offset, NULL, NULL);

    /* member_metadata */
    offset = dissect_kafka_bytes(subtree, hf_kafka_member_metadata, tvb, pinfo, offset, NULL, NULL);

    /* member_assignment */
    offset = dissect_kafka_bytes(subtree, hf_kafka_member_assignment, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    if (api_version < 4) {
        proto_item_append_text(subti, " (Member=%s)",
                               tvb_get_string_enc(wmem_packet_scope(), tvb,
                                                  member_start, member_len, ENC_UTF_8|ENC_NA));
    } else {
        proto_item_append_text(subti, " (Member=%s, Instance=%s)",
                               tvb_get_string_enc(wmem_packet_scope(), tvb,
                                                  member_start, member_len, ENC_UTF_8|ENC_NA),
                               tvb_get_string_enc(wmem_packet_scope(), tvb,
                                                  instance_start, instance_len, ENC_UTF_8|ENC_NA));
    }
    return offset;
}

static int
dissect_kafka_describe_groups_response_group(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                             kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int group_start, group_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group, &subti, "Group");

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    /* group_id */
    offset = dissect_kafka_string(subtree, hf_kafka_consumer_group, tvb, pinfo, offset,
                                  &group_start, &group_len);

    /* state */
    offset = dissect_kafka_string(subtree, hf_kafka_group_state, tvb, pinfo, offset, NULL, NULL);

    /* protocol_type */
    offset = dissect_kafka_string(subtree, hf_kafka_protocol_type, tvb, pinfo, offset, NULL, NULL);

    /* protocol */
    offset = dissect_kafka_string(subtree, hf_kafka_protocol_name, tvb, pinfo, offset, NULL, NULL);

    /* [member] */
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_group_members,
                                        &subsubti, "Members");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_groups_response_member);
    proto_item_set_end(subsubti, tvb, offset);

    if (api_version >= 3) {
        proto_tree_add_item(subtree, hf_kafka_group_authorized_ops, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Group=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              group_start, group_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_describe_groups_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                       kafka_api_version_t api_version)
{
    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* [group] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_groups_response_group);

    return offset;
}

/* LIST_GROUPS REQUEST/RESPONSE */

static int
dissect_kafka_list_groups_request(tvbuff_t *tvb _U_, packet_info *pinfo _U_, proto_tree *tree _U_, int offset,
                                  kafka_api_version_t api_version _U_)
{
    return offset;
}

static int
dissect_kafka_list_groups_response_group(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                         kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int group_start, group_len;
    int protocol_type_start, protocol_type_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group, &subti, "Group");

    /* group_id */
    offset = dissect_kafka_string(subtree, hf_kafka_consumer_group, tvb, pinfo, offset,
                                  &group_start, &group_len);

    /* protocol_type */
    offset = dissect_kafka_string(subtree, hf_kafka_protocol_type, tvb, pinfo, offset,
                                  &protocol_type_start, &protocol_type_len);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Group-ID=%s, Protocol-Type=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              group_start, group_len, ENC_UTF_8|ENC_NA),
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              protocol_type_start, protocol_type_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_list_groups_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                   kafka_api_version_t api_version)
{
    if (api_version >= 1) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    /* [group] */
    offset = dissect_kafka_array(tree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_list_groups_response_group);

    return offset;
}

/* SASL_HANDSHAKE REQUEST/RESPONSE */

static int
dissect_kafka_sasl_handshake_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version _U_)
{
    /* mechanism */
    offset = dissect_kafka_string(tree, hf_kafka_sasl_mechanism, tvb, pinfo, offset, NULL, NULL);

    return offset;
}

static int
dissect_kafka_sasl_handshake_response_enabled_mechanism(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                        int offset, kafka_api_version_t api_version _U_)
{
    /* enabled_mechanism */
    offset = dissect_kafka_string(tree, hf_kafka_sasl_mechanism, tvb, pinfo, offset, NULL, NULL);

    return offset;
}

static int
dissect_kafka_sasl_handshake_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                      kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    /* error_code */
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    /* [enabled_mechanism] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_sasl_enabled_mechanisms,
                                     &subti, "Enabled SASL Mechanisms");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_sasl_handshake_response_enabled_mechanism);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* CREATE_TOPICS REQUEST/RESPONSE */

static int
dissect_kafka_create_topics_request_replica(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                            int offset, kafka_api_version_t api_version _U_)
{
    /* replica */
    proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_create_topics_request_replica_assignment(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                       int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_replica_assignment,
                                     &subti, "Replica Assignment");

    /* partition_id */
    partition = (gint32) tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* [replica] */
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_topics_request_replica);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Partition-ID=%d)",
                           partition);

    return offset;
}

static int
dissect_kafka_create_topics_request_config(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                           int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int key_start, key_len;
    int val_start, val_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_config,
                                     &subti, "Config");

    /* key */
    offset = dissect_kafka_string(subtree, hf_kafka_config_key, tvb, pinfo, offset, &key_start, &key_len);

    /* value */
    offset = dissect_kafka_string(subtree, hf_kafka_config_value, tvb, pinfo, offset, &val_start, &val_len);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Key=%s, Value=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              key_start, key_len, ENC_UTF_8|ENC_NA),
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              val_start, val_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_create_topics_request_create_topic_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                         int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topic,
                                     &subti, "Create Topic Request");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    /* num_partitions */
    proto_tree_add_item(subtree, hf_kafka_num_partitions, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    /* replication_factor */
    proto_tree_add_item(subtree, hf_kafka_replication_factor, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    /* [replica_assignment] */
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_replica_assignment,
                                        &subsubti, "Replica Assignments");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_topics_request_replica_assignment);
    proto_item_set_end(subsubti, tvb, offset);

    /* [config] */
    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_config,
                                        &subsubti, "Configs");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_topics_request_config);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_create_topics_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    /* [topic] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Create Topic Requests");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_topics_request_create_topic_request);
    proto_item_set_end(subti, tvb, offset);

    /* timeout */
    proto_tree_add_item(tree, hf_kafka_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 1) {
        /* validate */
        proto_tree_add_item(tree, hf_kafka_validate_only, tvb, offset, 1, ENC_NA);
        offset += 1;
    }

    return offset;
}

static int
dissect_kafka_create_topics_response_topic_error_code(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;
    kafka_error_t error;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topic,
                                     &subti, "Topic Error Code");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    /* error_code */
    offset = dissect_kafka_error_ret(tvb, pinfo, subtree, offset, &error);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s, Error=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA),
                           kafka_error_to_str(error));

    return offset;
}

static int
dissect_kafka_create_topics_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    if (api_version >= 2) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* [topic_error_code] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topic Error Codes");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_topics_response_topic_error_code);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* DELETE_TOPICS REQUEST/RESPONSE */

static int
dissect_kafka_delete_topics_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                          int offset, kafka_api_version_t api_version _U_)
{
    /* topic */
    offset = dissect_kafka_string(tree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);

    return offset;
}

static int
dissect_kafka_delete_topics_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    /* [topic] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_topics_request_topic);
    proto_item_set_end(subti, tvb, offset);

    /* timeout */
    proto_tree_add_item(tree, hf_kafka_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_delete_topics_response_topic_error_code(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;
    kafka_error_t error;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topic,
                                     &subti, "Topic Error Code");

    /* topic */
    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    /* error_code */
    offset = dissect_kafka_error_ret(tvb, pinfo, subtree, offset, &error);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s, Error=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA),
                           kafka_error_to_str(error));

    return offset;
}

static int
dissect_kafka_delete_topics_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    if (api_version >= 3) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    /* [topic_error_code] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topic Error Codes");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_topics_response_topic_error_code);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* DELETE_RECORDS REQUEST/RESPONSE */

static int
dissect_kafka_delete_records_request_topic_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                           int offset, kafka_api_version_t api_version _U_)
{
    guint32 partition_id;
    gint64 partition_offset;
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    partition_offset = tvb_get_ntohi64(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_item_set_end(subti, tvb, offset);

    if (partition_offset == -1) {
        proto_item_append_text(subti, " (ID=%u, Offset=HWM)", partition_id);
    } else {
        proto_item_append_text(subti, " (ID=%u, Offset=%" G_GINT64_MODIFIER "i)", partition_id, partition_offset);
    }

    return offset;
}

static int
dissect_kafka_delete_records_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                          int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_records_request_topic_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_delete_records_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    /* [topic] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_records_request_topic);
    proto_item_set_end(subti, tvb, offset);

    /* timeout */
    proto_tree_add_item(tree, hf_kafka_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_delete_records_response_topic_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                     int offset, kafka_api_version_t api_version _U_)
{
    guint32 partition_id;
    gint64 partition_offset; // low watermark
    kafka_error_t partition_error_code;

    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    partition_offset = tvb_get_ntohi64(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    partition_error_code = (kafka_error_t) tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_error, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_item_set_end(subti, tvb, offset);

    if (partition_error_code == 0) {
        proto_item_append_text(subti, " (ID=%u, Offset=%" G_GINT64_MODIFIER "i)", partition_id, partition_offset);
    } else {
        proto_item_append_text(subti, " (ID=%u, Error=%s)", partition_id, kafka_error_to_str(partition_error_code));
    }

    return offset;
}

static int
dissect_kafka_delete_records_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                           int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_records_response_topic_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}


static int
dissect_kafka_delete_records_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    /* [topic_error_code] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_records_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* INIT_PRODUCER_ID REQUEST/RESPONSE */

static int
dissect_kafka_init_producer_id_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                              kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_string(tree, hf_kafka_transactional_id, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_transaction_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}


static int
dissect_kafka_init_producer_id_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                               kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    proto_tree_add_item(tree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(tree, hf_kafka_producer_epoch, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    return offset;
}

/* OFFSET_FOR_LEADER_EPOCH REQUEST/RESPONSE */

static int
dissect_kafka_offset_for_leader_epoch_request_topic_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                            int offset, kafka_api_version_t api_version)
{
    guint32 partition_id;
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 2) {
        proto_tree_add_item(subtree, hf_kafka_current_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_item_set_end(subti, tvb, offset);

    proto_item_append_text(subti, " (ID=%u)", partition_id);

    return offset;
}

static int
dissect_kafka_offset_for_leader_epoch_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                  int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_for_leader_epoch_request_topic_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_offset_for_leader_epoch_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                            kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;
    gint32 replica_id;

    if (api_version >= 3) {
        replica_id = tvb_get_ntohl(tvb, offset);
        subti = proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
        if (replica_id==-2) {
            proto_item_append_text(subti, " (debug)");
        } else if (replica_id==-1) {
            proto_item_append_text(subti, " (consumer)");
        }
        offset += 4;
    }

    /* [topic] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_for_leader_epoch_request_topic);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_offset_for_leader_epoch_response_topic_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                             int offset, kafka_api_version_t api_version _U_)
{
    guint32 partition_id;
    kafka_error_t partition_error_code;

    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_error_code = (kafka_error_t) tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_error, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    if (api_version >= 1) {
        proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    proto_tree_add_item(subtree, hf_kafka_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_item_set_end(subti, tvb, offset);

    if (partition_error_code == 0) {
        proto_item_append_text(subti, " (ID=%u)", partition_id);
    } else {
        proto_item_append_text(subti, " (ID=%u, Error=%s)", partition_id, kafka_error_to_str(partition_error_code));
    }

    return offset;
}

static int
dissect_kafka_offset_for_leader_epoch_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                   int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_for_leader_epoch_response_topic_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}


static int
dissect_kafka_offset_for_leader_epoch_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                             kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    if (api_version >= 2) {
        offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);
    }

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_offset_for_leader_epoch_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* ADD_PARTITIONS_TO_TXN REQUEST/RESPONSE */

static int
dissect_kafka_add_partitions_to_txn_request_topic_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                     int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);

    return offset+4;
}

static int
dissect_kafka_add_partitions_to_txn_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                           int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_add_partitions_to_txn_request_topic_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_add_partitions_to_txn_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_string(tree, hf_kafka_transactional_id, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(tree, hf_kafka_producer_epoch, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    /* [topic] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_add_partitions_to_txn_request_topic);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_add_partitions_to_txn_response_topic_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version _U_)
{
    guint32 partition_id;
    kafka_error_t partition_error_code;

    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    partition_error_code = (kafka_error_t) tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_error, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_item_set_end(subti, tvb, offset);

    if (partition_error_code == 0) {
        proto_item_append_text(subti, " (ID=%u)", partition_id);
    } else {
        proto_item_append_text(subti, " (ID=%u, Error=%s)", partition_id, kafka_error_to_str(partition_error_code));
    }

    return offset;
}

static int
dissect_kafka_add_partitions_to_txn_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                            int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_add_partitions_to_txn_response_topic_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}


static int
dissect_kafka_add_partitions_to_txn_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                      kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_add_partitions_to_txn_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* ADD_OFFSETS_TO_TXN REQUEST/RESPONSE */

static int
dissect_kafka_add_offsets_to_txn_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                              kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_string(tree, hf_kafka_transactional_id, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(tree, hf_kafka_producer_epoch, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset, NULL, NULL);

    return offset;
}


static int
dissect_kafka_add_offsets_to_txn_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                               kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    return offset;
}

/* END_TXN REQUEST/RESPONSE */

static int
dissect_kafka_end_txn_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                            kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_string(tree, hf_kafka_transactional_id, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(tree, hf_kafka_producer_epoch, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_tree_add_item(tree, hf_kafka_transaction_result, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    return offset;
}


static int
dissect_kafka_end_txn_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                             kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    return offset;
}

/* WRITE_TXN_MARKERS REQUEST/RESPONSE */

static int
dissect_kafka_write_txn_markers_request_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                     int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_write_txn_markers_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                           int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_write_txn_markers_request_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_write_txn_markers_request_marker(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    guint64 producer_id;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_marker,
                                     &subti, "Marker");

    producer_id = tvb_get_ntoh64(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(subtree, hf_kafka_producer_epoch, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_tree_add_item(subtree, hf_kafka_transaction_result, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subsubti, "Topics");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_write_txn_markers_request_topic);

    proto_tree_add_item(subsubtree, hf_kafka_coordinator_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_item_set_end(subsubti, tvb, offset);
    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Producer=%" G_GINT64_MODIFIER "u)", producer_id);

    return offset;
}

static int
dissect_kafka_write_txn_markers_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                                kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    /* [topic] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_markers,
                                     &subti, "Markers");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_write_txn_markers_request_marker);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_write_txn_markers_response_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version _U_)
{
    guint32 partition_id;
    kafka_error_t partition_error_code;

    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    partition_error_code = (kafka_error_t) tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_error, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_item_set_end(subti, tvb, offset);

    if (partition_error_code == 0) {
        proto_item_append_text(subti, " (ID=%u", partition_id);
    } else {
        proto_item_append_text(subti, " (ID=%u, Error=%s)", partition_id, kafka_error_to_str(partition_error_code));
    }

    return offset;
}

static int
dissect_kafka_write_txn_markers_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                            int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_write_txn_markers_response_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_write_txn_markers_response_marker(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                               int offset, kafka_api_version_t api_version _U_)
{
    guint64 producer_id;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_marker, &subti, "Marker");

    producer_id = tvb_get_ntoh64(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Topics");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_write_txn_markers_response_topic);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Producer=%" G_GINT64_MODIFIER "u)", producer_id);

    return offset;
}

static int
dissect_kafka_write_txn_markers_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                      kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_markers,
                                     &subti, "Markers");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_write_txn_markers_response_marker);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* TXN_OFFSET_COMMIT REQUEST/RESPONSE */

static int
dissect_kafka_txn_offset_commit_request_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                     int offset, kafka_api_version_t api_version _U_)
{
    guint32 partition_id;
    gint64 partition_offset;
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    partition_offset = tvb_get_ntohi64(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_offset, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    if (api_version >= 2) {
        proto_tree_add_item(subtree, hf_kafka_leader_epoch, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;
    }

    offset = dissect_kafka_string(subtree, hf_kafka_metadata, tvb, pinfo, offset, NULL, NULL);
    proto_item_set_end(subti, tvb, offset);

    proto_item_append_text(subti, " (ID=%u, Offset=%" G_GINT64_MODIFIER "i)", partition_id, partition_offset);

    return offset;
}

static int
dissect_kafka_txn_offset_commit_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                           int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_txn_offset_commit_request_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_txn_offset_commit_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_string(tree, hf_kafka_transactional_id, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_producer_id, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(tree, hf_kafka_producer_epoch, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_txn_offset_commit_request_topic);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_txn_offset_commit_response_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version _U_)
{
    guint32 partition_id;
    kafka_error_t partition_error_code;

    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    partition_error_code = (kafka_error_t) tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_error, tvb, offset, 2, ENC_BIG_ENDIAN);
    offset += 2;

    proto_item_set_end(subti, tvb, offset);

    if (partition_error_code == 0) {
        proto_item_append_text(subti, " (ID=%u)", partition_id);
    } else {
        proto_item_append_text(subti, " (ID=%u, Error=%s)", partition_id, kafka_error_to_str(partition_error_code));
    }

    return offset;
}

static int
dissect_kafka_txn_offset_commit_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                            int offset, kafka_api_version_t api_version _U_)
{
    int topic_start, topic_len;
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_txn_offset_commit_response_partition);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Topic=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}


static int
dissect_kafka_txn_offset_commit_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                      kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    /* [topic_error_code] */
    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_txn_offset_commit_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* DESCRIBE_ACLS REQUEST/RESPONSE */

static int
dissect_kafka_describe_acls_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version)
{
    proto_tree_add_item(tree, hf_kafka_acl_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(tree, hf_kafka_acl_resource_name, tvb, pinfo, offset, NULL, NULL);

    if (api_version >= 1) {
        proto_tree_add_item(tree, hf_kafka_acl_resource_pattern_type, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    offset = dissect_kafka_string(tree, hf_kafka_acl_principal, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_string(tree, hf_kafka_acl_host, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_acl_operation, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_kafka_acl_permission_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    return offset;
}

static int
dissect_kafka_describe_acls_response_resource_acl(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                   int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_acl, &subti, "ACL");

    offset = dissect_kafka_string(subtree, hf_kafka_acl_principal, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_string(subtree, hf_kafka_acl_host, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_acl_operation, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_tree_add_item(subtree, hf_kafka_acl_permission_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    return offset;
}

static int
dissect_kafka_describe_acls_response_resource(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                               int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Resource");

    proto_tree_add_item(subtree, hf_kafka_acl_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_acl_resource_name, tvb, pinfo, offset, NULL, NULL);

    if (api_version >= 1) {
        proto_tree_add_item(subtree, hf_kafka_acl_resource_pattern_type, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_acls, &subsubti, "ACLs");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_acls_response_resource_acl);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}


static int
dissect_kafka_describe_acls_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                         kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    offset = dissect_kafka_string(tree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_resources,
                                     &subti, "Resources");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_acls_response_resource);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* CREATE_ACLS REQUEST/RESPONSE */

static int
dissect_kafka_create_acls_request_creation(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                  int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_acl_creation, &subti, "Creation");

    proto_tree_add_item(subtree, hf_kafka_acl_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_acl_resource_name, tvb, pinfo, offset, NULL, NULL);

    if (api_version >= 1) {
        proto_tree_add_item(subtree, hf_kafka_acl_resource_pattern_type, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    offset = dissect_kafka_string(subtree, hf_kafka_acl_principal, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_string(subtree, hf_kafka_acl_host, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_acl_operation, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_tree_add_item(subtree, hf_kafka_acl_permission_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_create_acls_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_acl_creations,
                                     &subti, "Creations");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_acls_request_creation);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_create_acls_response_creation(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                   int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_acl_creation, &subti, "Creation");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    return offset;
}

static int
dissect_kafka_create_acls_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                         kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_acl_creations,
                                     &subti, "Creations");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_acls_response_creation);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* DELETE_ACLS REQUEST/RESPONSE */

static int
dissect_kafka_delete_acls_request_filter(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                           int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_acl_filter, &subti, "Filter");

    proto_tree_add_item(subtree, hf_kafka_acl_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_acl_resource_name, tvb, pinfo, offset, NULL, NULL);

    if (api_version >= 1) {
        proto_tree_add_item(subtree, hf_kafka_acl_resource_pattern_type, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    offset = dissect_kafka_string(subtree, hf_kafka_acl_principal, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_string(subtree, hf_kafka_acl_host, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_acl_operation, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_tree_add_item(subtree, hf_kafka_acl_permission_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_delete_acls_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                  kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_acl_filter,
                                     &subti, "Filters");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_acls_request_filter);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_delete_acls_response_match(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                          int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_acl_filter_match, &subti, "Match");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_acl_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_acl_resource_name, tvb, pinfo, offset, NULL, NULL);

    if (api_version >= 1) {
        proto_tree_add_item(subtree, hf_kafka_acl_resource_pattern_type, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    offset = dissect_kafka_string(subtree, hf_kafka_acl_principal, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_string(subtree, hf_kafka_acl_host, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_acl_operation, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_tree_add_item(subtree, hf_kafka_acl_permission_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_delete_acls_response_filter(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                            int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_acl_creation, &subti, "Filter");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                     ett_kafka_acl_filter_matches,
                                     &subsubti, "Matches");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_acls_response_match);

    proto_item_set_end(subsubti, tvb, offset);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_delete_acls_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                   kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_acl_creations,
                                     &subti, "Filters");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_acls_response_filter);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* DESCRIBE_CONFIGS REQUEST/RESPONSE */

static int
dissect_kafka_describe_config_request_entry(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                               int offset, kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_string(tree, hf_kafka_config_key, tvb, pinfo, offset, NULL, NULL);

    return offset;
}

static int
dissect_kafka_describe_config_request_resource(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                         int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Resource");

    proto_tree_add_item(subtree, hf_kafka_config_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_config_resource_name, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_config_entries, &subsubti, "Entries");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_config_request_entry);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_describe_configs_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                  kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_resources,
                                     &subti, "Resources");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_config_request_resource);

    if (api_version >= 1) {
        proto_tree_add_item(subtree, hf_kafka_config_include_synonyms, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_describe_configs_response_synonym(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int key_start, key_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_config_synonym, &subti, "Synonym");

    offset = dissect_kafka_string(subtree, hf_kafka_config_key, tvb, pinfo, offset, &key_start, &key_len);
    offset = dissect_kafka_string(subtree, hf_kafka_config_value, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_config_source, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Key=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              key_start, key_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_describe_configs_response_entry(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                         int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int key_start, key_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_config_entry, &subti, "Entry");

    offset = dissect_kafka_string(subtree, hf_kafka_config_key, tvb, pinfo, offset, &key_start, &key_len);
    offset = dissect_kafka_string(subtree, hf_kafka_config_value, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_config_readonly, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    if (api_version == 0) {
        proto_tree_add_item(subtree, hf_kafka_config_default, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    } else {
        proto_tree_add_item(subtree, hf_kafka_config_source, tvb, offset, 1, ENC_BIG_ENDIAN);
        offset += 1;
    }

    proto_tree_add_item(subtree, hf_kafka_config_sensitive, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    if (api_version >= 1) {
        subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                            ett_kafka_config_synonyms,
                                            &subsubti, "Synonyms");
        offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                     &dissect_kafka_describe_configs_response_synonym);

        proto_item_set_end(subsubti, tvb, offset);
    }

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Key=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              key_start, key_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_describe_configs_response_resource(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                          int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Resource");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_config_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_config_resource_name, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_config_entries,
                                        &subsubti, "Entries");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_configs_response_entry);

    proto_item_set_end(subsubti, tvb, offset);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_describe_configs_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                   kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_resources,
                                     &subti, "Resources");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_configs_response_resource);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* ALTER_CONFIGS REQUEST/RESPONSE */

static int
dissect_kafka_alter_config_request_entry(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                            int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_config_entry, &subti, "Entry");

    offset = dissect_kafka_string(subtree, hf_kafka_config_key, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_string(subtree, hf_kafka_config_value, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_config_request_resource(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                               int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Resource");

    proto_tree_add_item(subtree, hf_kafka_config_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_config_resource_name, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_config_entries, &subsubti, "Entries");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_config_request_entry);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_configs_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                       kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_resources,
                                     &subti, "Resources");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_config_request_resource);

    proto_tree_add_item(subtree, hf_kafka_validate_only, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_configs_response_resource(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                 int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Resource");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_config_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_config_resource_name, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_configs_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_resources,
                                     &subti, "Resources");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_configs_response_resource);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* ALTER_REPLICA_LOG_DIRS REQUEST/RESPONSE */

static int
dissect_kafka_alter_replica_log_dirs_request_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                         int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_alter_replica_log_dirs_request_topic(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                 int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topics, &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_replica_log_dirs_request_partition);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_alter_replica_log_dirs_request_log_dir(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                            int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_log_dir, &subti, "Log Directory");

    offset = dissect_kafka_string(subtree, hf_kafka_log_dir, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topics, &subsubti, "Topics");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_replica_log_dirs_request_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_replica_log_dirs_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_log_dirs,
                                     &subti, "Log Directories");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_replica_log_dirs_request_log_dir);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_replica_log_dirs_response_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                       int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int partition_id;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    proto_item_append_text(subti, " (ID=%u)", partition_id);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_replica_log_dirs_response_topic(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_log_dir, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partition");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_replica_log_dirs_response_partition);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_alter_replica_log_dirs_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_replica_log_dirs_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* DESCRIBE_LOG_DIRS REQUEST/RESPONSE */

static int
dissect_kafka_describe_log_dirs_request_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                       int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_describe_log_dirs_request_topic(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                   int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_log_dirs_request_partition);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_describe_log_dirs_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                             kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_log_dirs_request_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_describe_log_dirs_response_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                        int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    int partition_id;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    partition_id = tvb_get_ntohl(tvb, offset);
    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_tree_add_item(subtree, hf_kafka_segment_size, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(subtree, hf_kafka_offset_lag, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    proto_tree_add_item(subtree, hf_kafka_future, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (ID=%u)", partition_id);

    return offset;
}

static int
dissect_kafka_describe_log_dirs_response_topic(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                    int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_log_dirs_response_partition);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_describe_log_dirs_response_log_dir(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                    int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int dir_start, dir_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_log_dir, &subti, "Log Directory");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_log_dir, tvb, pinfo, offset, &dir_start, &dir_len);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_topics, &subsubti, "Topics");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_log_dirs_response_topic);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Dir=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              dir_start, dir_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_describe_log_dirs_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                              kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_log_dirs,
                                     &subti, "Log Directories");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_log_dirs_response_log_dir);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* CREATE_PARTITIONS REQUEST/RESPONSE */

static int
dissect_kafka_create_partitions_request_broker(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                  int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_broker_nodeid, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_create_partitions_request_topic(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    proto_tree_add_item(subtree, hf_kafka_partition_count, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1, ett_kafka_brokers, &subsubti, "Brokers");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_partitions_request_broker);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_create_partitions_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_partitions_request_topic);

    proto_item_set_end(subti, tvb, offset);

    proto_tree_add_item(tree, hf_kafka_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_tree_add_item(tree, hf_kafka_validate_only, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    return offset;
}

static int
dissect_kafka_create_partitions_response_topic(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                               int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    int topic_start, topic_len;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, &topic_start, &topic_len);

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);
    proto_item_append_text(subti, " (Name=%s)",
                           tvb_get_string_enc(wmem_packet_scope(), tvb,
                                              topic_start, topic_len, ENC_UTF_8|ENC_NA));

    return offset;
}

static int
dissect_kafka_create_partitions_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                         kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_partitions_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* SASL_AUTHENTICATE REQUEST/RESPONSE */

static int
dissect_kafka_sasl_authenticate_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                         kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_bytes(tree, hf_kafka_sasl_auth_bytes, tvb, pinfo, offset, NULL, NULL);

    return offset;
}


static int
dissect_kafka_sasl_authenticate_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                          kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    offset = dissect_kafka_string(tree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_bytes(tree, hf_kafka_sasl_auth_bytes, tvb, pinfo, offset, NULL, NULL);

    if (api_version >= 1) {
        proto_tree_add_item(tree, hf_kafka_session_lifetime_ms, tvb, offset, 8, ENC_BIG_ENDIAN);
        offset += 8;
    }

    return offset;
}

/* CREATE_DELEGATION_TOKEN REQUEST/RESPONSE */

static int
dissect_kafka_create_delegation_token_request_renewer(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_renewer, &subti, "Renewer");

    offset = dissect_kafka_string(subtree, hf_kafka_token_principal_type, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_string(subtree, hf_kafka_token_principal_name, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_create_delegation_token_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_renewers,
                                     &subti, "Renewers");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_create_delegation_token_request_renewer);

    proto_item_set_end(subti, tvb, offset);

    proto_tree_add_item(tree, hf_kafka_token_max_life_time, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    return offset;
}

static int
dissect_kafka_create_delegation_token_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                         kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    offset = dissect_kafka_string(tree, hf_kafka_token_principal_type, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_string(tree, hf_kafka_token_principal_name, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_token_issue_timestamp, tvb, offset, 8, ENC_TIME_MSECS | ENC_BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(tree, hf_kafka_token_expiry_timestamp, tvb, offset, 8, ENC_TIME_MSECS | ENC_BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(tree, hf_kafka_token_max_timestamp, tvb, offset, 8, ENC_TIME_MSECS | ENC_BIG_ENDIAN);
    offset += 8;

    offset = dissect_kafka_string(tree, hf_kafka_token_id, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_bytes(tree, hf_kafka_token_hmac, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    return offset;
}

/* RENEW_DELEGATION_TOKEN REQUEST/RESPONSE */

static int
dissect_kafka_renew_delegation_token_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                              kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_bytes(tree, hf_kafka_token_hmac, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_token_renew_time, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    return offset;
}

static int
dissect_kafka_renew_delegation_token_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                               kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);
    proto_tree_add_item(tree, hf_kafka_token_expiry_timestamp, tvb, offset, 8, ENC_TIME_MSECS | ENC_BIG_ENDIAN);
    offset += 8;
    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    return offset;
}

/* EXPIRE_DELEGATION_TOKEN REQUEST/RESPONSE */

static int
dissect_kafka_expire_delegation_token_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                             kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_bytes(tree, hf_kafka_token_hmac, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(tree, hf_kafka_token_expiry_time, tvb, offset, 8, ENC_BIG_ENDIAN);
    offset += 8;

    return offset;
}

static int
dissect_kafka_expire_delegation_token_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                              kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_error(tvb, pinfo, tree, offset);
    proto_tree_add_item(tree, hf_kafka_token_expiry_timestamp, tvb, offset, 8, ENC_TIME_MSECS | ENC_BIG_ENDIAN);
    offset += 8;
    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    return offset;
}

/* DESCRIBE_DELEGATION_TOKEN REQUEST/RESPONSE */

static int
dissect_kafka_describe_delegation_token_request_owner(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_owner, &subti, "Owner");

    offset = dissect_kafka_string(subtree, hf_kafka_token_principal_type, tvb, pinfo, offset, NULL, NULL);

    offset = dissect_kafka_string(subtree, hf_kafka_token_principal_name, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_describe_delegation_token_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                              kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_owners,
                                     &subti, "Owners");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_delegation_token_request_owner);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_describe_delegation_token_response_renewer(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                     int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_renewer, &subti, "Renewer");

    offset = dissect_kafka_string(subtree, hf_kafka_token_principal_type, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_string(subtree, hf_kafka_token_principal_name, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_describe_delegation_token_response_token(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_token, &subti, "Token");

    offset = dissect_kafka_string(subtree, hf_kafka_token_principal_type, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_string(subtree, hf_kafka_token_principal_name, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_token_issue_timestamp, tvb, offset, 8, ENC_TIME_MSECS | ENC_BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(subtree, hf_kafka_token_expiry_timestamp, tvb, offset, 8, ENC_TIME_MSECS | ENC_BIG_ENDIAN);
    offset += 8;
    proto_tree_add_item(subtree, hf_kafka_token_max_timestamp, tvb, offset, 8, ENC_TIME_MSECS | ENC_BIG_ENDIAN);
    offset += 8;

    offset = dissect_kafka_string(subtree, hf_kafka_token_id, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_bytes(subtree, hf_kafka_token_hmac, tvb, pinfo, offset, NULL, NULL);


    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                     ett_kafka_renewers,
                                     &subsubti, "Renewers");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_delegation_token_response_renewer);

    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_describe_delegation_token_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                               kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_tokens,
                                     &subti, "Tokens");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_describe_delegation_token_response_token);

    proto_item_set_end(subti, tvb, offset);

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    return offset;
}

/* DELETE_GROUPS REQUEST/RESPONSE */

static int
dissect_kafka_delete_groups_request_group(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                      int offset, kafka_api_version_t api_version _U_)
{
    offset = dissect_kafka_string(tree, hf_kafka_consumer_group, tvb, pinfo, offset, NULL, NULL);

    return offset;
}

static int
dissect_kafka_delete_groups_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                                kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_groups,
                                     &subti, "Groups");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_groups_request_group);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_delete_groups_response_group(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                         int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_group, &subti, "Group");

    offset = dissect_kafka_string(subtree, hf_kafka_consumer_group, tvb, pinfo, offset, NULL, NULL);
    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_delete_groups_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                                 kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_groups,
                                     &subti, "Groups");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_delete_groups_response_group);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* ELECT_LEADERS REQUEST/RESPONSE */

static int
dissect_kafka_elect_leaders_request_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                          int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_elect_leaders_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topic,
                                     &subti, "Topic");

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                     ett_kafka_partitions,
                                     &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_elect_leaders_request_partition);

    proto_item_set_end(subsubti, tvb, offset);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_elect_leaders_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    if (api_version >= 1) {
        proto_tree_add_item(tree, hf_kafka_election_type, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 1;
    }

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_elect_leaders_request_topic);

    proto_item_set_end(subti, tvb, offset);

    proto_tree_add_item(tree, hf_kafka_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_elect_leaders_response_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                  int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_partition,
                                     &subti, "Partition");

    proto_tree_add_item(subtree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_elect_leaders_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                              kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topic,
                                     &subti, "Topic");

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_partitions,
                                        &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_elect_leaders_response_partition);

    proto_item_set_end(subsubti, tvb, offset);
    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_elect_leaders_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    if (api_version >= 1) {
        offset = dissect_kafka_error(tvb, pinfo, tree, offset);
    }

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");

    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_elect_leaders_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* INCREMENTAL_ALTER_CONFIGS REQUEST/RESPONSE */

static int
dissect_kafka_inc_alter_config_request_entry(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                         int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_config_entry, &subti, "Entry");

    offset = dissect_kafka_string(subtree, hf_kafka_config_key, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_config_operation, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_config_value, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_inc_alter_config_request_resource(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                            int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Resource");

    proto_tree_add_item(subtree, hf_kafka_config_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_config_resource_name, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_config_entries, &subsubti, "Entries");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_inc_alter_config_request_entry);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_inc_alter_configs_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                    kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_resources,
                                     &subti, "Resources");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_inc_alter_config_request_resource);

    proto_tree_add_item(subtree, hf_kafka_validate_only, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_inc_alter_configs_response_resource(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                              int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_resource, &subti, "Resource");

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    proto_tree_add_item(subtree, hf_kafka_config_resource_type, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset += 1;

    offset = dissect_kafka_string(subtree, hf_kafka_config_resource_name, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_inc_alter_configs_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                     kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_resources,
                                     &subti, "Resources");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_inc_alter_configs_response_resource);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* ALTER_PARTITION_REASSIGNMENTS REQUEST/RESPONSE */

static int
dissect_kafka_alter_partition_reassignments_request_replica(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                             int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_alter_partition_reassignments_request_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                          int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &partition);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Replicas");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_partition_reassignments_request_replica);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_partition_reassignments_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_partition_reassignments_request_partition);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_partition_reassignments_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                        kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    proto_tree_add_item(tree, hf_kafka_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_partition_reassignments_request_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_partition_reassignments_response_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                              int offset, kafka_api_version_t api_version _U_)
{
    proto_item *subti;
    proto_tree *subtree;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &partition);

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_partition_reassignments_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                          int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_partition_reassignments_response_partition);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_alter_partition_reassignments_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                                    kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    offset = dissect_kafka_string(tree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_alter_partition_reassignments_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* LIST_PARTITION_REASSIGNMENTS REQUEST/RESPONSE */

static int
dissect_kafka_list_partition_reassignments_request_partition(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                              int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_partition_id, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_list_partition_reassignments_request_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                          int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_list_partition_reassignments_request_partition);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_list_partition_reassignments_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                                    kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    proto_tree_add_item(tree, hf_kafka_timeout, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_list_partition_reassignments_request_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_list_partition_reassignments_response_replica(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree,
                                                             int offset, kafka_api_version_t api_version _U_)
{
    proto_tree_add_item(tree, hf_kafka_replica, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    return offset;
}

static int
dissect_kafka_list_partition_reassignments_response_partition(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                               int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;
    kafka_partition_t partition;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partition, &subti, "Partition");

    offset = dissect_kafka_partition_id_ret(tvb, pinfo, subtree, offset, &partition);

    offset = dissect_kafka_error(tvb, pinfo, subtree, offset);

    offset = dissect_kafka_string(subtree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_replicas,
                                        &subsubti, "Current Replicas");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_list_partition_reassignments_response_replica);
    proto_item_set_end(subsubti, tvb, offset);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_replicas,
                                        &subsubti, "Adding Replicas");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_list_partition_reassignments_response_replica);
    proto_item_set_end(subsubti, tvb, offset);

    subsubtree = proto_tree_add_subtree(subtree, tvb, offset, -1,
                                        ett_kafka_replicas,
                                        &subsubti, "Removing Replicas");
    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_list_partition_reassignments_response_replica);
    proto_item_set_end(subsubti, tvb, offset);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_list_partition_reassignments_response_topic(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                                           int offset, kafka_api_version_t api_version)
{
    proto_item *subti, *subsubti;
    proto_tree *subtree, *subsubtree;

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_topic, &subti, "Topic");

    offset = dissect_kafka_string(subtree, hf_kafka_topic_name, tvb, pinfo, offset, NULL, NULL);

    subsubtree = proto_tree_add_subtree(tree, tvb, offset, -1, ett_kafka_partitions, &subsubti, "Partitions");

    offset = dissect_kafka_array(subsubtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_list_partition_reassignments_response_partition);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

static int
dissect_kafka_list_partition_reassignments_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset,
                                                     kafka_api_version_t api_version)
{
    proto_item *subti;
    proto_tree *subtree;

    offset = dissect_kafka_throttle_time(tvb, pinfo, tree, offset);

    offset = dissect_kafka_error(tvb, pinfo, tree, offset);

    offset = dissect_kafka_string(tree, hf_kafka_error_message, tvb, pinfo, offset, NULL, NULL);

    subtree = proto_tree_add_subtree(tree, tvb, offset, -1,
                                     ett_kafka_topics,
                                     &subti, "Topics");
    offset = dissect_kafka_array(subtree, tvb, pinfo, offset, api_version,
                                 &dissect_kafka_list_partition_reassignments_response_topic);

    proto_item_set_end(subti, tvb, offset);

    return offset;
}

/* MAIN */

static int
dissect_kafka(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    proto_item             *root_ti, *ti;
    proto_tree             *kafka_tree;
    int                     offset  = 0;
    kafka_query_response_t *matcher = NULL;
    conversation_t         *conversation;
    wmem_queue_t           *match_queue;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "Kafka");
    col_clear(pinfo->cinfo, COL_INFO);

    root_ti = proto_tree_add_item(tree, proto_kafka, tvb, 0, -1, ENC_NA);

    kafka_tree = proto_item_add_subtree(root_ti, ett_kafka);

    proto_tree_add_item(kafka_tree, hf_kafka_len, tvb, offset, 4, ENC_BIG_ENDIAN);
    offset += 4;

    conversation = find_or_create_conversation(pinfo);
    /* Create match_queue for this conversation */
    match_queue  = (wmem_queue_t *) conversation_get_proto_data(conversation, proto_kafka);
    if (match_queue == NULL) {
        match_queue = wmem_queue_new(wmem_file_scope());
        conversation_add_proto_data(conversation, proto_kafka, match_queue);
    }

    if (PINFO_FD_VISITED(pinfo)) {
        matcher = (kafka_query_response_t *) p_get_proto_data(wmem_file_scope(), pinfo, proto_kafka, 0);
    }

    if (pinfo->destport == pinfo->match_uint) {
        /* Request (as directed towards server port) */
        if (matcher == NULL) {
            matcher = wmem_new(wmem_file_scope(), kafka_query_response_t);

            matcher->api_key        = tvb_get_ntohs(tvb, offset);
            matcher->api_version    = tvb_get_ntohs(tvb, offset+2);
            matcher->request_frame  = pinfo->num;
            matcher->response_found = FALSE;

            p_add_proto_data(wmem_file_scope(), pinfo, proto_kafka, 0, matcher);

            /* The kafka server always responds, except in the case of a produce
             * request whose RequiredAcks field is 0. This field is at a dynamic
             * offset into the request, so to avoid too much prefetch logic we
             * simply don't queue produce requests here. If it is a produce
             * request with a non-zero RequiredAcks field it gets queued later.
             */
            if (matcher->api_key != KAFKA_PRODUCE) {
                wmem_queue_push(match_queue, matcher);
            }
        }

        col_add_fstr(pinfo->cinfo, COL_INFO, "Kafka %s v%d Request",
                     kafka_api_key_to_str(matcher->api_key),
                     matcher->api_version);
        /* Also add to protocol root */
        proto_item_append_text(root_ti, " (%s v%d Request)",
                               kafka_api_key_to_str(matcher->api_key),
                               matcher->api_version);

        if (matcher->response_found) {
            ti = proto_tree_add_uint(kafka_tree, hf_kafka_response_frame, tvb,
                    0, 0, matcher->response_frame);
            proto_item_set_generated(ti);
        }


        ti = proto_tree_add_item(kafka_tree, hf_kafka_request_api_key, tvb, offset, 2, ENC_BIG_ENDIAN);
        proto_item_set_hidden(ti);

        ti = proto_tree_add_item(kafka_tree, hf_kafka_api_key, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;
        kafka_check_supported_api_key(pinfo, ti, matcher);

        ti = proto_tree_add_item(kafka_tree, hf_kafka_request_api_version, tvb, offset, 2, ENC_BIG_ENDIAN);
        proto_item_set_hidden(ti);

        ti = proto_tree_add_item(kafka_tree, hf_kafka_api_version, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;
        kafka_check_supported_api_version(pinfo, ti, matcher);

        proto_tree_add_item(kafka_tree, hf_kafka_correlation_id, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;

        offset = dissect_kafka_string(kafka_tree, hf_kafka_client_id, tvb, pinfo, offset, NULL, NULL);

        switch (matcher->api_key) {
            case KAFKA_PRODUCE:
                /* Produce requests may need delayed queueing, see the more
                 * detailed comment above. */
                if (tvb_get_ntohs(tvb, offset) != KAFKA_ACK_NOT_REQUIRED && !PINFO_FD_VISITED(pinfo)) {
                    wmem_queue_push(match_queue, matcher);
                }
                /*offset =*/ dissect_kafka_produce_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_FETCH:
                /*offset =*/ dissect_kafka_fetch_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_OFFSETS:
                /*offset =*/ dissect_kafka_offsets_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_METADATA:
                /*offset =*/ dissect_kafka_metadata_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_LEADER_AND_ISR:
                /*offset =*/ dissect_kafka_leader_and_isr_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_STOP_REPLICA:
                /*offset =*/ dissect_kafka_stop_replica_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_UPDATE_METADATA:
                /*offset =*/ dissect_kafka_update_metadata_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CONTROLLED_SHUTDOWN:
                /*offset =*/ dissect_kafka_controlled_shutdown_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_OFFSET_COMMIT:
                /*offset =*/ dissect_kafka_offset_commit_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_OFFSET_FETCH:
                /*offset =*/ dissect_kafka_offset_fetch_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_FIND_COORDINATOR:
                /*offset =*/ dissect_kafka_find_coordinator_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_JOIN_GROUP:
                /*offset =*/ dissect_kafka_join_group_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_HEARTBEAT:
                /*offset =*/ dissect_kafka_heartbeat_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_LEAVE_GROUP:
                /*offset =*/ dissect_kafka_leave_group_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_SYNC_GROUP:
                /*offset =*/ dissect_kafka_sync_group_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_GROUPS:
                /*offset =*/ dissect_kafka_describe_groups_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_LIST_GROUPS:
                /*offset =*/ dissect_kafka_list_groups_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_SASL_HANDSHAKE:
                /*offset =*/ dissect_kafka_sasl_handshake_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_API_VERSIONS:
                /*offset =*/ dissect_kafka_api_versions_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CREATE_TOPICS:
                /*offset =*/ dissect_kafka_create_topics_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DELETE_TOPICS:
                /*offset =*/ dissect_kafka_delete_topics_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DELETE_RECORDS:
                /*offset =*/ dissect_kafka_delete_records_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_INIT_PRODUCER_ID:
                /*offset =*/ dissect_kafka_init_producer_id_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_OFFSET_FOR_LEADER_EPOCH:
                /*offset =*/ dissect_kafka_offset_for_leader_epoch_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ADD_PARTITIONS_TO_TXN:
                /*offset =*/ dissect_kafka_add_partitions_to_txn_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ADD_OFFSETS_TO_TXN:
                /*offset =*/ dissect_kafka_add_offsets_to_txn_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_END_TXN:
                /*offset =*/ dissect_kafka_end_txn_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_WRITE_TXN_MARKERS:
                /*offset =*/ dissect_kafka_write_txn_markers_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_TXN_OFFSET_COMMIT:
                /*offset =*/ dissect_kafka_txn_offset_commit_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_ACLS:
                /*offset =*/ dissect_kafka_describe_acls_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CREATE_ACLS:
                /*offset =*/ dissect_kafka_create_acls_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DELETE_ACLS:
                /*offset =*/ dissect_kafka_delete_acls_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_CONFIGS:
                /*offset =*/ dissect_kafka_describe_configs_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ALTER_CONFIGS:
                /*offset =*/ dissect_kafka_alter_configs_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ALTER_REPLICA_LOG_DIRS:
                /*offset =*/ dissect_kafka_alter_replica_log_dirs_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_LOG_DIRS:
                /*offset =*/ dissect_kafka_describe_log_dirs_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CREATE_PARTITIONS:
                /*offset =*/ dissect_kafka_create_partitions_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_SASL_AUTHENTICATE:
                /*offset =*/ dissect_kafka_sasl_authenticate_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CREATE_DELEGATION_TOKEN:
                /*offset =*/ dissect_kafka_create_delegation_token_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_RENEW_DELEGATION_TOKEN:
                /*offset =*/ dissect_kafka_renew_delegation_token_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_EXPIRE_DELEGATION_TOKEN:
                /*offset =*/ dissect_kafka_expire_delegation_token_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_DELEGATION_TOKEN:
                /*offset =*/ dissect_kafka_describe_delegation_token_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DELETE_GROUPS:
                /*offset =*/ dissect_kafka_delete_groups_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ELECT_LEADERS:
                /*offset =*/ dissect_kafka_elect_leaders_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_INC_ALTER_CONFIGS:
                /*offset =*/ dissect_kafka_inc_alter_configs_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ALTER_PARTITION_REASSIGNMENTS:
                /*offset =*/ dissect_kafka_alter_partition_reassignments_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_LIST_PARTITION_REASSIGNMENTS:
                /*offset =*/ dissect_kafka_list_partition_reassignments_request(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
        }
    }
    else {
        /* Response */

        proto_tree_add_item(kafka_tree, hf_kafka_correlation_id, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;

        if (matcher == NULL) {
            if (wmem_queue_count(match_queue) > 0) {
                matcher = (kafka_query_response_t *) wmem_queue_peek(match_queue);
            }
            if (matcher == NULL || matcher->request_frame >= pinfo->num) {
                col_set_str(pinfo->cinfo, COL_INFO, "Kafka Response (Undecoded, Request Missing)");
                expert_add_info(pinfo, root_ti, &ei_kafka_request_missing);
                return tvb_captured_length(tvb);
            }

            wmem_queue_pop(match_queue);

            matcher->response_frame = pinfo->num;
            matcher->response_found = TRUE;

            p_add_proto_data(wmem_file_scope(), pinfo, proto_kafka, 0, matcher);
        }

        col_add_fstr(pinfo->cinfo, COL_INFO, "Kafka %s v%d Response",
                     kafka_api_key_to_str(matcher->api_key),
                     matcher->api_version);
        /* Also add to protocol root */
        proto_item_append_text(root_ti, " (%s v%d Response)",
                               kafka_api_key_to_str(matcher->api_key),
                               matcher->api_version);


        /* Show request frame */
        ti = proto_tree_add_uint(kafka_tree, hf_kafka_request_frame, tvb,
                0, 0, matcher->request_frame);
        proto_item_set_generated(ti);

        /* Show api key (message type) */
        ti = proto_tree_add_int(kafka_tree, hf_kafka_response_api_key, tvb,
                0, 0, matcher->api_key);
        proto_item_set_generated(ti);
        proto_item_set_hidden(ti);
        ti = proto_tree_add_int(kafka_tree, hf_kafka_api_key, tvb,
                                0, 0, matcher->api_key);
        proto_item_set_generated(ti);
        kafka_check_supported_api_key(pinfo, ti, matcher);

        /* Also show api version from request */
        ti = proto_tree_add_int(kafka_tree, hf_kafka_response_api_version, tvb,
                0, 0, matcher->api_version);
        proto_item_set_generated(ti);
        proto_item_set_hidden(ti);
        ti = proto_tree_add_int(kafka_tree, hf_kafka_response_api_version, tvb,
                                0, 0, matcher->api_version);
        proto_item_set_generated(ti);
        kafka_check_supported_api_version(pinfo, ti, matcher);

        switch (matcher->api_key) {
            case KAFKA_PRODUCE:
                /*offset =*/ dissect_kafka_produce_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_FETCH:
                /*offset =*/ dissect_kafka_fetch_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_OFFSETS:
                /*offset =*/ dissect_kafka_offsets_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_METADATA:
                /*offset =*/ dissect_kafka_metadata_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_LEADER_AND_ISR:
                /*offset =*/ dissect_kafka_leader_and_isr_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_STOP_REPLICA:
                /*offset =*/ dissect_kafka_stop_replica_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_UPDATE_METADATA:
                /*offset =*/ dissect_kafka_update_metadata_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CONTROLLED_SHUTDOWN:
                /*offset =*/ dissect_kafka_controlled_shutdown_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_OFFSET_COMMIT:
                /*offset =*/ dissect_kafka_offset_commit_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_OFFSET_FETCH:
                /*offset =*/ dissect_kafka_offset_fetch_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_FIND_COORDINATOR:
                /*offset =*/ dissect_kafka_find_coordinator_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_JOIN_GROUP:
                /*offset =*/ dissect_kafka_join_group_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_HEARTBEAT:
                /*offset =*/ dissect_kafka_heartbeat_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_LEAVE_GROUP:
                /*offset =*/ dissect_kafka_leave_group_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_SYNC_GROUP:
                /*offset =*/ dissect_kafka_sync_group_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_GROUPS:
                /*offset =*/ dissect_kafka_describe_groups_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_LIST_GROUPS:
                /*offset =*/ dissect_kafka_list_groups_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_SASL_HANDSHAKE:
                /*offset =*/ dissect_kafka_sasl_handshake_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_API_VERSIONS:
                /*offset =*/ dissect_kafka_api_versions_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CREATE_TOPICS:
                /*offset =*/ dissect_kafka_create_topics_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DELETE_TOPICS:
                /*offset =*/ dissect_kafka_delete_topics_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DELETE_RECORDS:
                /*offset =*/ dissect_kafka_delete_records_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_INIT_PRODUCER_ID:
                /*offset =*/ dissect_kafka_init_producer_id_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_OFFSET_FOR_LEADER_EPOCH:
                /*offset =*/ dissect_kafka_offset_for_leader_epoch_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ADD_PARTITIONS_TO_TXN:
                /*offset =*/ dissect_kafka_add_partitions_to_txn_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ADD_OFFSETS_TO_TXN:
                /*offset =*/ dissect_kafka_add_offsets_to_txn_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_END_TXN:
                /*offset =*/ dissect_kafka_end_txn_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_WRITE_TXN_MARKERS:
                /*offset =*/ dissect_kafka_write_txn_markers_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_TXN_OFFSET_COMMIT:
                /*offset =*/ dissect_kafka_txn_offset_commit_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_ACLS:
                /*offset =*/ dissect_kafka_describe_acls_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CREATE_ACLS:
                /*offset =*/ dissect_kafka_create_acls_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DELETE_ACLS:
                /*offset =*/ dissect_kafka_delete_acls_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_CONFIGS:
                /*offset =*/ dissect_kafka_describe_configs_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ALTER_CONFIGS:
                /*offset =*/ dissect_kafka_alter_configs_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ALTER_REPLICA_LOG_DIRS:
                /*offset =*/ dissect_kafka_alter_replica_log_dirs_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_LOG_DIRS:
                /*offset =*/ dissect_kafka_describe_log_dirs_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CREATE_PARTITIONS:
                /*offset =*/ dissect_kafka_create_partitions_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_SASL_AUTHENTICATE:
                /*offset =*/ dissect_kafka_sasl_authenticate_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_CREATE_DELEGATION_TOKEN:
                /*offset =*/ dissect_kafka_create_delegation_token_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_RENEW_DELEGATION_TOKEN:
                /*offset =*/ dissect_kafka_renew_delegation_token_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_EXPIRE_DELEGATION_TOKEN:
                /*offset =*/ dissect_kafka_expire_delegation_token_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DESCRIBE_DELEGATION_TOKEN:
                /*offset =*/ dissect_kafka_describe_delegation_token_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_DELETE_GROUPS:
                /*offset =*/ dissect_kafka_delete_groups_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ELECT_LEADERS:
                /*offset =*/ dissect_kafka_elect_leaders_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_INC_ALTER_CONFIGS:
                /*offset =*/ dissect_kafka_inc_alter_configs_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_ALTER_PARTITION_REASSIGNMENTS:
                /*offset =*/ dissect_kafka_alter_partition_reassignments_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
            case KAFKA_LIST_PARTITION_REASSIGNMENTS:
                /*offset =*/ dissect_kafka_list_partition_reassignments_response(tvb, pinfo, kafka_tree, offset, matcher->api_version);
                break;
        }

    }

    return tvb_captured_length(tvb);
}

static int
dissect_kafka_tcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
        void *data)
{
    tcp_dissect_pdus(tvb, pinfo, tree, TRUE, 4,
            get_kafka_pdu_len, dissect_kafka, data);

    return tvb_captured_length(tvb);
}

static void
compute_kafka_api_names(void)
{
    guint i;
    guint len = array_length(kafka_apis);

    for (i = 0; i < len; ++i) {
        kafka_api_names[i].value  = kafka_apis[i].api_key;
        kafka_api_names[i].strptr = kafka_apis[i].name;
    }

    kafka_api_names[len].value  = 0;
    kafka_api_names[len].strptr = NULL;
}

void
proto_register_kafka(void)
{
    static hf_register_info hf[] = {
        { &hf_kafka_len,
            { "Length", "kafka.len",
               FT_INT32, BASE_DEC, 0, 0,
              "The length of this Kafka packet.", HFILL }
        },
        { &hf_kafka_offset,
            { "Offset", "kafka.offset",
               FT_INT64, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_offset_time,
            { "Time", "kafka.offset_time",
               FT_INT64, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_log_start_offset,
            { "Log Start Offset", "kafka.log_start_offset",
               FT_INT64, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_last_stable_offset,
            { "Last Stable Offset", "kafka.last_stable_offset",
               FT_INT64, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_first_offset,
            { "First Offset", "kafka.first_offset",
               FT_INT64, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_max_offsets,
            { "Max Offsets", "kafka.max_offsets",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_metadata,
            { "Metadata", "kafka.metadata",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_error,
            { "Error", "kafka.error",
               FT_INT16, BASE_DEC, VALS(kafka_errors), 0,
               NULL, HFILL }
        },
        { &hf_kafka_error_message,
            { "Error Message", "kafka.error_message",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_api_key,
            { "API Key", "kafka.api_key",
                FT_INT16, BASE_DEC, VALS(kafka_api_names), 0,
                "Request API Key.", HFILL }
        },
        { &hf_kafka_api_version,
            { "API Version", "kafka.api_version",
                FT_INT16, BASE_DEC, 0, 0,
                "Request API Version.", HFILL }
        },
        // these should be deprecated
        // --- begin ---
        { &hf_kafka_request_api_key,
            { "API Key", "kafka.request_key",
               FT_INT16, BASE_DEC, VALS(kafka_api_names), 0,
              "Request API.", HFILL }
        },
        { &hf_kafka_response_api_key,
            { "API Key", "kafka.response_key",
               FT_INT16, BASE_DEC, VALS(kafka_api_names), 0,
              "Response API.", HFILL }
        },
        { &hf_kafka_request_api_version,
            { "API Version", "kafka.request.version",
               FT_INT16, BASE_DEC, 0, 0,
              "Request API Version.", HFILL }
        },
        { &hf_kafka_response_api_version,
            { "API Version", "kafka.response.version",
               FT_INT16, BASE_DEC, 0, 0,
              "Response API Version.", HFILL }
        },
        // --- end ---
        { &hf_kafka_correlation_id,
            { "Correlation ID", "kafka.correlation_id",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_client_id,
            { "Client ID", "kafka.client_id",
               FT_STRING, STR_ASCII, 0, 0,
              "The ID of the sending client.", HFILL }
        },
        { &hf_kafka_client_host,
            { "Client Host", "kafka.client_host",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_transactional_id,
            { "Transactional ID", "kafka.transactional_id",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_transaction_result,
            { "Transaction Result", "kafka.transaction_result",
               FT_INT8, BASE_DEC, VALS(kafka_transaction_results), 0,
               NULL, HFILL }
        },
        { &hf_kafka_transaction_timeout,
            { "Transaction Timeout", "kafka.transaction_timeout",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_string_len,
            { "String Length", "kafka.string_len",
               FT_INT16, BASE_DEC, 0, 0,
              "Generic length for kafka-encoded string.", HFILL }
        },
        { &hf_kafka_bytes_len,
            { "Bytes Length", "kafka.bytes_len",
               FT_INT32, BASE_DEC, 0, 0,
              "Generic length for kafka-encoded bytes.", HFILL }
        },
        { &hf_kafka_required_acks,
            { "Required Acks", "kafka.required_acks",
               FT_INT16, BASE_DEC, VALS(kafka_acks), 0,
               NULL, HFILL }
        },
        { &hf_kafka_timeout,
            { "Timeout", "kafka.timeout",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_topic_name,
            { "Topic Name", "kafka.topic_name",
               FT_STRING, STR_UNICODE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_producer_id,
            { "Producer ID", "kafka.producer_id",
               FT_INT64, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_producer_epoch,
            { "Producer Epoch", "kafka.producer_epoch",
                FT_INT16, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_partition_id,
            { "Partition ID", "kafka.partition_id",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_replica,
            { "Replica ID", "kafka.replica_id",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_replication_factor,
            { "Replication Factor", "kafka.replication_factor",
               FT_INT16, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_isr,
            { "Caught-Up Replica ID", "kafka.isr_id",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_offline,
            { "Offline Replica ID", "kafka.offline_id",
                FT_INT32, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_message_size,
            { "Message Size", "kafka.message_size",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_message_crc,
            { "CRC32", "kafka.message_crc",
               FT_UINT32, BASE_HEX, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_message_magic,
            { "Magic Byte", "kafka.message_magic",
               FT_INT8, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_message_codec,
            { "Compression Codec", "kafka.message_codec",
               FT_UINT8, BASE_DEC, VALS(kafka_message_codecs), KAFKA_MESSAGE_CODEC_MASK,
               NULL, HFILL }
        },
        { &hf_kafka_message_timestamp_type,
            { "Timestamp Type", "kafka.message_timestamp_type",
               FT_UINT8, BASE_DEC, VALS(kafka_message_timestamp_types), KAFKA_MESSAGE_TIMESTAMP_MASK,
               NULL, HFILL }
        },
        { &hf_kafka_batch_crc,
            { "CRC32", "kafka.batch_crc",
                FT_UINT32, BASE_HEX, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_batch_codec,
            { "Compression Codec", "kafka.batch_codec",
                FT_UINT16, BASE_DEC, VALS(kafka_message_codecs), KAFKA_MESSAGE_CODEC_MASK,
                NULL, HFILL }
        },
        { &hf_kafka_batch_timestamp_type,
            { "Timestamp Type", "kafka.batch_timestamp_type",
                FT_UINT16, BASE_DEC, VALS(kafka_message_timestamp_types), KAFKA_MESSAGE_TIMESTAMP_MASK,
                NULL, HFILL }
        },
        { &hf_kafka_batch_transactional,
            { "Transactional", "kafka.batch_transactional",
                FT_UINT16, BASE_DEC, VALS(kafka_batch_transactional_values), KAFKA_BATCH_TRANSACTIONAL_MASK,
                NULL, HFILL }
        },
        { &hf_kafka_batch_control_batch,
            { "Control Batch", "kafka.batch_control_batch",
                FT_UINT16, BASE_DEC, VALS(kafka_batch_control_batch_values), KAFKA_BATCH_CONTROL_BATCH_MASK,
                NULL, HFILL }
        },
        { &hf_kafka_batch_last_offset_delta,
            { "Last Offset Delta", "kafka.batch_last_offset_delta",
               FT_UINT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_batch_first_timestamp,
            { "First Timestamp", "kafka.batch_first_timestamp",
                FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC, NULL, 0,
                NULL, HFILL }
        },
        { &hf_kafka_batch_last_timestamp,
            { "Last Timestamp", "kafka.batch_last_timestamp",
                FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC, NULL, 0,
                NULL, HFILL }
        },
        { &hf_kafka_batch_base_sequence,
            { "Base Sequence", "kafka.batch_base_sequence",
                FT_INT32, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_batch_size,
            { "Size", "kafka.batch_size",
                FT_UINT32, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_message_timestamp,
            { "Timestamp", "kafka.message_timestamp",
               FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC, NULL, 0,
               NULL, HFILL }
        },
        { &hf_kafka_message_key,
            { "Key", "kafka.message_key",
               FT_BYTES, BASE_SHOW_ASCII_PRINTABLE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_message_value,
            { "Value", "kafka.message_value",
               FT_BYTES, BASE_SHOW_ASCII_PRINTABLE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_message_compression_reduction,
            { "Compression Reduction (compressed/uncompressed)", "kafka.message_compression_reduction",
               FT_FLOAT, BASE_NONE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_consumer_group,
            { "Consumer Group", "kafka.consumer_group",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_consumer_group_instance,
            { "Consumer Group Instance", "kafka.consumer_group_instance",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_coordinator_key,
            { "Coordinator Key", "kafka.coordinator_key",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_coordinator_type,
            { "Coordinator Type", "kafka.coordinator_type",
               FT_INT8, BASE_DEC, VALS(kafka_coordinator_types), 0,
               NULL, HFILL }
        },
        { &hf_kafka_request_frame,
            { "Request Frame", "kafka.request_frame",
               FT_FRAMENUM, BASE_NONE, FRAMENUM_TYPE(FT_FRAMENUM_REQUEST), 0,
               NULL, HFILL }
        },
        { &hf_kafka_broker_nodeid,
            { "Node ID", "kafka.node_id",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_broker_epoch,
            { "Broker Epoch", "kafka.broker_epoch",
               FT_INT64, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_broker_host,
            { "Host", "kafka.host",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_listener_name,
            { "Listener", "kafka.listener_name",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_broker_port,
            { "Port", "kafka.port",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_rack,
            { "Rack", "kafka.rack",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_broker_security_protocol_type,
            { "Security Protocol Type", "kafka.broker_security_protocol_type",
               FT_INT16, BASE_DEC, VALS(kafka_security_protocol_types), 0,
               NULL, HFILL }
        },
        { &hf_kafka_cluster_id,
            { "Cluster ID", "kafka.cluster_id",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_controller_id,
            { "Controller ID", "kafka.node_id",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_controller_epoch,
            { "Controller Epoch", "kafka.controller_epoch",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_delete_partitions,
            { "Delete Partitions", "kafka.delete_partitions",
               FT_BOOLEAN, BASE_NONE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_group_leader_id,
            { "Leader ID", "kafka.group_leader_id",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_leader_id,
            { "Leader ID", "kafka.leader_id",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_leader_epoch,
            { "Leader Epoch", "kafka.leader_epoch",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_current_leader_epoch,
            { "Leader Epoch", "kafka.current_leader_epoch",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_is_internal,
            { "Is Internal", "kafka.is_internal",
               FT_BOOLEAN, BASE_NONE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_min_bytes,
            { "Min Bytes", "kafka.min_bytes",
               FT_INT32, BASE_DEC, 0, 0,
               "The minimum number of bytes of messages that must be available"
                   " to give a response.",
               HFILL }
        },
        { &hf_kafka_max_bytes,
            { "Max Bytes", "kafka.max_bytes",
               FT_INT32, BASE_DEC, 0, 0,
               "The maximum bytes to include in the message set for this"
                   " partition. This helps bound the size of the response.",
               HFILL }
        },
        { &hf_kafka_isolation_level,
            { "Isolation Level", "kafka.isolation_level",
               FT_INT8, BASE_DEC, VALS(kafka_isolation_levels), 0,
               NULL, HFILL }
        },
        { &hf_kafka_max_wait_time,
            { "Max Wait Time", "kafka.max_wait_time",
               FT_INT32, BASE_DEC, 0, 0,
               "The maximum amount of time in milliseconds to block waiting if"
                   " insufficient data is available at the time the request is"
                   " issued.",
               HFILL }
        },
        { &hf_kafka_throttle_time,
            { "Throttle time", "kafka.throttle_time",
               FT_INT32, BASE_DEC, 0, 0,
               "Duration in milliseconds for which the request was throttled"
                   " due to quota violation."
                   " (Zero if the request did not violate any quota.)",
               HFILL }
        },
        { &hf_kafka_response_frame,
            { "Response Frame", "kafka.response_frame",
               FT_FRAMENUM, BASE_NONE, FRAMENUM_TYPE(FT_FRAMENUM_RESPONSE), 0,
               NULL, HFILL }
        },
        { &hf_kafka_api_versions_api_key,
            { "API Key", "kafka.api_versions.api_key",
               FT_INT16, BASE_DEC, VALS(kafka_api_names), 0,
              "API Key.", HFILL }
        },
        { &hf_kafka_api_versions_min_version,
            { "Min Version", "kafka.api_versions.min_version",
               FT_INT16, BASE_DEC, 0, 0,
              "Minimal version which supports api key.", HFILL }
        },
        { &hf_kafka_api_versions_max_version,
            { "Max Version", "kafka.api_versions.max_version",
              FT_INT16, BASE_DEC, 0, 0,
              "Maximal version which supports api key.", HFILL }
        },
        { &hf_kafka_session_timeout,
            { "Session Timeout", "kafka.session_timeout",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_rebalance_timeout,
            { "Rebalance Timeout", "kafka.rebalance_timeout",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_group_state,
            { "State", "kafka.group_state",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_member_id,
            { "Consumer Group Member ID", "kafka.member_id",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_protocol_type,
            { "Protocol Type", "kafka.protocol_type",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_protocol_name,
            { "Protocol Name", "kafka.protocol_name",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_protocol_metadata,
            { "Protocol Metadata", "kafka.protocol_metadata",
               FT_BYTES, BASE_NONE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_member_metadata,
            { "Member Metadata", "kafka.member_metadata",
               FT_BYTES, BASE_NONE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_generation_id,
            { "Generation ID", "kafka.generation_id",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_member_assignment,
            { "Member Assignment", "kafka.member_assignment",
               FT_BYTES, BASE_NONE, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_sasl_mechanism,
            { "SASL Mechanism", "kafka.sasl_mechanism",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_num_partitions,
            { "Number of Partitions", "kafka.num_partitions",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_zk_version,
            { "Zookeeper Version", "kafka.zk_version",
               FT_INT32, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_config_key,
            { "Key", "kafka.config_key",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_config_value,
            { "Value", "kafka.config_value",
               FT_STRING, STR_ASCII, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_config_operation,
            { "Operation", "kafka.config_operation",
               FT_INT8, BASE_DEC, VALS(config_operations), 0,
               NULL, HFILL }
        },
        { &hf_kafka_commit_timestamp,
            { "Timestamp", "kafka.commit_timestamp",
               FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC, NULL, 0,
               NULL, HFILL }
        },
        { &hf_kafka_retention_time,
            { "Retention Time", "kafka.retention_time",
               FT_INT64, BASE_DEC, 0, 0,
               NULL, HFILL }
        },
        { &hf_kafka_forgotten_topic_name,
            { "Forgotten Topic Name", "kafka.forgotten_topic_name",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_forgotten_topic_partition,
            { "Forgotten Topic Partition", "kafka.forgotten_topic_partition",
                FT_INT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_fetch_session_id,
            { "Fetch Session ID", "kafka.fetch_session_id",
                FT_INT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_fetch_session_epoch,
            { "Fetch Session Epoch", "kafka.fetch_session_epoch",
                FT_INT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_record_header_key,
            { "Header Key", "kafka.header_key",
                FT_STRING, STR_UNICODE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_record_header_value,
            { "Header Value", "kafka.header_value",
                FT_BYTES, BASE_SHOW_ASCII_PRINTABLE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_record_attributes,
            { "Record Attributes (reserved)", "kafka.record_attributes",
                FT_INT8, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_allow_auto_topic_creation,
            { "Allow Auto Topic Creation", "kafka.allow_auto_topic_creation",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_validate_only,
            { "Only Validate the Request", "kafka.validate_only",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_coordinator_epoch,
            { "Coordinator Epoch", "kafka.coordinator_epoch",
                FT_INT32, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_sasl_auth_bytes,
            { "SASL Authentication Bytes", "kafka.sasl_authentication",
                FT_BYTES, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_session_lifetime_ms,
            { "Session Lifetime (ms)", "kafka.session_lifetime_ms",
                FT_INT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_acl_resource_type,
            { "Resource Type", "kafka.acl_resource_type",
                FT_INT8, BASE_DEC, VALS(acl_resource_types), 0,
                NULL, HFILL }
        },
        { &hf_kafka_acl_resource_name,
            { "Resource Name", "kafka.acl_resource_name",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_acl_resource_pattern_type,
            { "Resource Pattern Type", "kafka.acl_resource_pattern_type",
                FT_INT8, BASE_DEC, VALS(acl_resource_pattern_types), 0,
                NULL, HFILL }
        },
        { &hf_kafka_acl_principal,
            { "Principal", "kafka.acl_principal",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_acl_host,
            { "Host", "kafka.acl_host",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_acl_operation,
            { "Operation", "kafka.acl_operation",
                FT_INT8, BASE_DEC, VALS(acl_operations), 0,
                NULL, HFILL }
        },
        { &hf_kafka_acl_permission_type,
            { "Permission Type", "kafka.acl_permission_type",
                FT_INT8, BASE_DEC, VALS(acl_permission_types), 0,
                NULL, HFILL }
        },
        { &hf_kafka_config_resource_type,
            { "Resource Type", "kafka.config_resource_type",
                FT_INT8, BASE_DEC, VALS(config_resource_types), 0,
                NULL, HFILL }
        },
        { &hf_kafka_config_resource_name,
            { "Resource Name", "kafka.config_resource_name",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_config_include_synonyms,
            { "Include Synonyms", "kafka.config_include_synonyms",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_config_default,
            { "Default", "kafka.config_default",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_config_readonly,
            { "Readonly", "kafka.config_readonly",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_config_sensitive,
            { "Sensitive", "kafka.config_sensitive",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_config_source,
            { "Source", "kafka.config_source",
                FT_INT8, BASE_DEC, VALS(config_sources), 0,
                NULL, HFILL }
        },
        { &hf_kafka_log_dir,
            { "Log Directory", "kafka.log_dir",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_segment_size,
            { "Segment Size", "kafka.segment_size",
                FT_UINT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_offset_lag,
            { "Offset Lag", "kafka.offset_lag",
                FT_UINT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_future,
            { "Future", "kafka.future",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_partition_count,
            { "Partition Count", "kafka.partition_count",
                FT_UINT32, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_max_life_time,
            { "Max Life Time", "kafka.token_max_life_time",
                FT_INT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_renew_time,
            { "Renew Time", "kafka.renew_time",
                FT_INT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_expiry_time,
            { "Expiry Time", "kafka.expiry_time",
                FT_INT64, BASE_DEC, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_principal_type,
            { "Principal Type", "kafka.principal_type",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_principal_name,
            { "Principal Name", "kafka.principal_name",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_issue_timestamp,
            { "Issue Timestamp", "kafka.token_issue_timestamp",
                FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC, NULL, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_expiry_timestamp,
            { "Expiry Timestamp", "kafka.token_expiry_timestamp",
                FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC, NULL, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_max_timestamp,
            { "Max Timestamp", "kafka.token_max_timestamp",
                FT_ABSOLUTE_TIME, ABSOLUTE_TIME_UTC, NULL, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_id,
            { "ID", "kafka.token_id",
                FT_STRING, STR_ASCII, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_token_hmac,
            { "HMAC", "kafka.token_hmac",
                FT_BYTES, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_include_cluster_authorized_ops,
            { "Include Cluster Authorized Operations", "kafka.include_cluster_authorized_ops",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_include_topic_authorized_ops,
            { "Include Topic Authorized Operations", "kafka.include_topic_authorized_ops",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_cluster_authorized_ops,
            { "Cluster Authorized Operations", "kafka.cluster_authorized_ops",
                FT_UINT32, BASE_HEX, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_topic_authorized_ops,
            { "Topic Authorized Operations", "kafka.topic_authorized_ops",
                FT_UINT32, BASE_HEX, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_include_group_authorized_ops,
            { "Include Group Authorized Operations", "kafka.include_group_authorized_ops",
                FT_BOOLEAN, BASE_NONE, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_group_authorized_ops,
            { "Group Authorized Operations", "kafka.group_authorized_ops",
                FT_UINT32, BASE_HEX, 0, 0,
                NULL, HFILL }
        },
        { &hf_kafka_election_type,
            { "ElectionType", "kafka.election_type",
                FT_INT8, BASE_DEC, VALS(election_types), 0,
                NULL, HFILL }
        },
    };

    static int *ett[] = {
        &ett_kafka,
        &ett_kafka_batch,
        &ett_kafka_message,
        &ett_kafka_message_set,
        &ett_kafka_offline,
        &ett_kafka_isrs,
        &ett_kafka_replicas,
        &ett_kafka_broker,
        &ett_kafka_brokers,
        &ett_kafka_broker_end_point,
        &ett_kafka_markers,
        &ett_kafka_marker,
        &ett_kafka_topics,
        &ett_kafka_topic,
        &ett_kafka_partitions,
        &ett_kafka_partition,
        &ett_kafka_api_version,
        &ett_kafka_group_protocols,
        &ett_kafka_group_protocol,
        &ett_kafka_group_members,
        &ett_kafka_group_member,
        &ett_kafka_group_assignments,
        &ett_kafka_group_assignment,
        &ett_kafka_groups,
        &ett_kafka_group,
        &ett_kafka_sasl_enabled_mechanisms,
        &ett_kafka_replica_assignment,
        &ett_kafka_configs,
        &ett_kafka_config,
        &ett_kafka_request_forgotten_topic,
        &ett_kafka_record,
        &ett_kafka_record_headers,
        &ett_kafka_record_headers_header,
        &ett_kafka_aborted_transactions,
        &ett_kafka_aborted_transaction,
        &ett_kafka_resources,
        &ett_kafka_resource,
        &ett_kafka_acls,
        &ett_kafka_acl,
        &ett_kafka_acl_creations,
        &ett_kafka_acl_creation,
        &ett_kafka_acl_filters,
        &ett_kafka_acl_filter,
        &ett_kafka_acl_filter_matches,
        &ett_kafka_acl_filter_match,
        &ett_kafka_config_synonyms,
        &ett_kafka_config_synonym,
        &ett_kafka_config_entries,
        &ett_kafka_config_entry,
        &ett_kafka_log_dirs,
        &ett_kafka_log_dir,
        &ett_kafka_renewers,
        &ett_kafka_renewer,
        &ett_kafka_owners,
        &ett_kafka_owner,
        &ett_kafka_tokens,
        &ett_kafka_token,
    };

    static ei_register_info ei[] = {
        { &ei_kafka_request_missing,
          { "kafka.request_missing", PI_UNDECODED, PI_WARN, "Request missing", EXPFILL }},
        { &ei_kafka_unknown_api_key,
          { "kafka.unknown_api_key", PI_UNDECODED, PI_WARN, "Unknown API key", EXPFILL }},
        { &ei_kafka_unsupported_api_version,
          { "kafka.unsupported_api_version", PI_UNDECODED, PI_WARN, "Unsupported API version", EXPFILL }},
        { &ei_kafka_bad_string_length,
          { "kafka.bad_string_length", PI_MALFORMED, PI_WARN, "Invalid string length field", EXPFILL }},
        { &ei_kafka_bad_bytes_length,
          { "kafka.bad_bytes_length", PI_MALFORMED, PI_WARN, "Invalid byte length field", EXPFILL }},
        { &ei_kafka_bad_array_length,
          { "kafka.bad_array_length", PI_MALFORMED, PI_WARN, "Invalid array length field", EXPFILL }},
        { &ei_kafka_bad_record_length,
          { "kafka.bad_record_length", PI_MALFORMED, PI_WARN, "Invalid record length field", EXPFILL }},
        { &ei_kafka_bad_varint,
          { "kafka.bad_varint", PI_MALFORMED, PI_WARN, "Invalid varint bytes", EXPFILL }},
        { &ei_kafka_bad_message_set_length,
          { "kafka.ei_kafka_bad_message_set_length", PI_MALFORMED, PI_WARN, "Message set size does not match content", EXPFILL }},
        { &ei_kafka_unknown_message_magic,
          { "kafka.unknown_message_magic", PI_MALFORMED, PI_WARN, "Invalid message magic field", EXPFILL }},
    };

    module_t *kafka_module;
    expert_module_t* expert_kafka;

    proto_kafka = proto_register_protocol("Kafka", "Kafka", "kafka");

    compute_kafka_api_names();
    proto_register_field_array(proto_kafka, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
    expert_kafka = expert_register_protocol(proto_kafka);
    expert_register_field_array(expert_kafka, ei, array_length(ei));

    kafka_module = prefs_register_protocol(proto_kafka, NULL);
    kafka_handle = register_dissector("kafka", dissect_kafka_tcp, proto_kafka);

    prefs_register_bool_preference(kafka_module, "show_string_bytes_lengths",
        "Show length for string and bytes fields in the protocol tree",
        "",
        &kafka_show_string_bytes_lengths);
}

void
proto_reg_handoff_kafka(void)
{
    dissector_add_uint_range_with_preference("tcp.port", KAFKA_TCP_DEFAULT_RANGE, kafka_handle);
    ssl_dissector_add(0, kafka_handle);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
