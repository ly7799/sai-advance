# Copyright 2013-present Centec Networks, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Thrift SAI MPLS VPN tests
"""
import socket
from switch import *
import sai_base_test
from ptf.mask import Mask
#pdb.set_trace()

class Scenario001_MplsVpnTestVpls(sai_base_test.ThriftInterfaceDataPlane):
    def runTest(self):
        """
        1.Set provider routing configuration
        2.Set provider mpls lsp configuration
        3.Set provider mpls pw configuration
        4.Set Provider mpls uni port configuration
        5.Set FDB entry
        8.Get and Set attribute
        9.Send encap and decap traffic packet,check packet receiving
        10.Clear configuration
        11.Repeat 1-10 for 3 times
        """
        sys_logging("1.Set provider routing configuration")
        switch_init(self.client)
        provider_vlan_list = [100]
        provider_vlan_oid_list = [0 for i in range(1)]
        provider_vlan_member_oid_list = [0 for i in range(1)]
        for num in range(1):
            vlan_id = provider_vlan_list[num]
            vlan_oid = sai_thrift_create_vlan(self.client, vlan_id)
            provider_vlan_oid_list[num] = vlan_oid
            tmp_port = port_list[num]
            vlan_member = sai_thrift_create_vlan_member(self.client, vlan_oid, tmp_port, SAI_VLAN_TAGGING_MODE_TAGGED)
            provider_vlan_member_oid_list[num] = vlan_member
        v4_enabled = 1
        v6_enabled = 1
        default_vrf_oid = 0x0000000003
        mac=router_mac
        provider_rif_oid_list = [0 for i in range(1)]
        for num in range(1):
            tmp_vlan_oid = provider_vlan_oid_list[num]
            provider_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_VLAN, 0, tmp_vlan_oid, v4_enabled, v6_enabled, mac)
            provider_rif_oid_list[num] = provider_rif_oid
        addr_family = SAI_IP_ADDR_FAMILY_IPV4
        ip_mask_32 = '255.255.255.255'
        ip_mask_24 = '255.255.255.0'
        provider_rif_ip_addr_list = ['192.168.1.1']
        provider_nexthop_ip_addr_list = ['192.168.1.2']
        provider_nexthop_mac_list = ['00:11:22:33:44:01']
        provider_nhop_oid_list = [0 for i in range(1)]
        mac_action = SAI_PACKET_ACTION_FORWARD
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0, packet_action=SAI_PACKET_ACTION_TRAP)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            ret = sai_thrift_create_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_neighbor failed!!! ret = %d" % ret)
            provider_nhop_oid = sai_thrift_create_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid)
            provider_nhop_oid_list[num] = provider_nhop_oid
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            sai_thrift_create_fdb(self.client, vlan_oid, nexthop_mac, tmp_port, mac_action)
        sys_logging("2.Set provider mpls lsp configuration")
        mpls_if_mac = ''
        mpls_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        nhp_lsp_label = 200
        nhp_lsp_label_for_nhp = (nhp_lsp_label<<12) | 64
        nhp_lsp_label_list = [nhp_lsp_label_for_nhp]
        nexthop_ip_addr = provider_nexthop_ip_addr_list[0]
        provider_rif_oid = provider_rif_oid_list[0]
        
        nhop_lsp_pe1_to_p = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, nhp_lsp_label_list)
        
        inseg_lsp_label = 100
        pop_nums = 1
        inseg_nhop_lsp = mpls_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_lsp_label, pop_nums, None, inseg_nhop_lsp, packet_action)
        
        sys_logging("3.Set provider mpls pw configuration")
        bridge_id = sai_thrift_create_bridge(self.client, SAI_BRIDGE_TYPE_1D)
        sys_logging(">>bridge_id = %d" % bridge_id)
        
        pw2_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw2_label = 102
        nhp_pw2_label = 202
        nhp_pw2_label_for_list = (nhp_pw2_label<<12) | 64
        
        label_list = [nhp_pw2_label_for_list]
        
        tunnel_id_pw2 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=False, encap_cw_en=False, encap_ttl_val=0, encap_tagged_vlan=201)
        nhop_pw_pe1_to_pe2 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw2, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        
        
        pop_nums = 1
        inseg_nhop_pw2 = pw2_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_pw2_label, pop_nums, None, inseg_nhop_pw2, packet_action, tunnel_id=tunnel_id_pw2)
        pw2_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw2, bridge_id=bridge_id)
        
        pw3_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw3_label = 103
        nhp_pw3_label = 202
        nhp_pw3_label_for_list = (nhp_pw3_label<<12) | 64
        label_list = [nhp_pw3_label_for_list]

        tunnel_id_pw3 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=False, encap_cw_en=False, encap_ttl_val=0, encap_tagged_vlan=301)
        nhop_pw_pe1_to_pe3 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw3, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw3 = pw3_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw3_label, pop_nums, None, inseg_nhop_pw3, packet_action, tunnel_id=tunnel_id_pw3)
        pw3_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw3, bridge_id=bridge_id)

        pw4_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw4_label = 104
        nhp_pw4_label = 202
        nhp_pw4_label_for_list = (nhp_pw4_label<<12) | 64
        label_list = [nhp_pw4_label_for_list]

        tunnel_id_pw4 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=False, encap_cw_en=False, encap_ttl_val=0, encap_tagged_vlan=401)
        nhop_pw_pe1_to_pe4 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw4, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw4 = pw4_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw4_label, pop_nums, None, inseg_nhop_pw4, packet_action, tunnel_id=tunnel_id_pw4)
        pw4_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw4, bridge_id=bridge_id)

        bport_pw_oid_list=[pw2_tunnel_bport_oid, pw3_tunnel_bport_oid, pw4_tunnel_bport_oid]
        nhp_pw_label_list=[nhp_pw2_label, nhp_pw3_label, nhp_pw4_label]
        inseg_pw_label_list=[inseg_pw2_label,inseg_pw3_label,inseg_pw4_label]
        sys_logging("4.Set Provider mpls uni port configuration")
        uni_vlan_id_list = [1001,1002]
        uni_port_oid_list=[0 for i in range(2)]
        tagged_vlan_list=[200,300,400]
        
        for num in range(2):
            uni_port=port_list[num+1]
            uni_vlan_id = uni_vlan_id_list[num]
            uni_port_oid = sai_thrift_create_bridge_sub_port(self.client, uni_port, bridge_id, uni_vlan_id)
            uni_port_oid_list[num] = uni_port_oid
        sys_logging("5.Set FDB entry")
        mac_host_remote_list = ['00:88:88:99:01:01','00:88:88:99:02:01','00:88:88:99:03:01']
        mac_host_local_list = ['00:88:88:88:01:01','00:88:88:88:02:01']
        ip_host_remote_list = ['1.1.1.102','1.1.1.103','1.1.1.104']
        ip_host_local_list = ['1.1.1.1','1.1.1.2']
        mac_action = SAI_PACKET_ACTION_FORWARD
        
        for num in range(3):
            mac_host_remote = mac_host_remote_list[num]
            bport_pw_oid = bport_pw_oid_list[num]
            sai_thrift_create_fdb_bport(self.client, bridge_id, mac_host_remote, bport_pw_oid, mac_action)
        for num in range(2):
            uni_port_oid = uni_port_oid_list[num]
            mac_host_local = mac_host_local_list[num]
            sai_thrift_create_fdb_bport(self.client, bridge_id, mac_host_local, uni_port_oid, mac_action)

        sys_logging("8.Get and Set attribute")
        ids_list = [SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE,SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW,SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE,SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW,SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN,SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID,SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID,SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID]
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw2,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN:
                u16 = ctypes.c_uint16(attribute.value.u16)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN = %d ###"  % u16.value)
                assert ( 201 == u16.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW = %d ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID = %x ###"  %attribute.value.oid)
                assert ( nhop_pw_pe1_to_pe2 == attribute.value.oid )
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw3,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN:
                u16 = ctypes.c_uint16(attribute.value.u16)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN = %g ###"  %u16.value)
                assert ( 301 == u16.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE = %g ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE = %g ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID = %x ###"  %attribute.value.oid)
                assert ( nhop_pw_pe1_to_pe3 == attribute.value.oid )
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw4,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN:
                u16 = ctypes.c_uint16(attribute.value.u16)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN = %g ###"  %u16.value)
                assert ( 401 == u16.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE = %g ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE = %g ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID = %x ###"  %attribute.value.oid)
                assert ( nhop_pw_pe1_to_pe4 == attribute.value.oid )
                
        u16 = ctypes.c_int16(200)
        attr_value = sai_thrift_attribute_value_t(u16=u16.value)
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw2,attr) 
        
        u16 = ctypes.c_int16(300)
        attr_value = sai_thrift_attribute_value_t(u16=u16.value)
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw3,attr) 
        
        u16 = ctypes.c_int16(400)
        attr_value = sai_thrift_attribute_value_t(u16=u16.value)
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw4,attr) 
        
        attr_value = sai_thrift_attribute_value_t(booldata=True)
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw2,attr) 
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw2,attr) 
        
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw3,attr) 
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw3,attr) 
        
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw4,attr) 
        attr = sai_thrift_attribute_t(id=SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW , value=attr_value)
        self.client.sai_thrift_set_tunnel_attribute(tunnel_id_pw4,attr) 
        
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw2,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN:
                u16 = ctypes.c_uint16(attribute.value.u16)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN = %d ###"  % u16.value)
                assert ( 200 == u16.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW = %d ###"  %attribute.value.booldata)
                assert ( True == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( True == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID = %x ###"  %attribute.value.oid)
                assert ( nhop_pw_pe1_to_pe2 == attribute.value.oid )
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw3,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN:
                u16 = ctypes.c_uint16(attribute.value.u16)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN = %g ###"  %u16.value)
                assert ( 300 == u16.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE = %g ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE = %g ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( True == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( True == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID = %x ###"  %attribute.value.oid)
                assert ( nhop_pw_pe1_to_pe3 == attribute.value.oid )
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw4,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN:
                u16 = ctypes.c_uint16(attribute.value.u16)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_TAGGED_VLAN = %g ###"  %u16.value)
                assert ( 400 == u16.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_MODE = %g ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_MODE = %g ###"  % u8.value)
                assert ( SAI_TUNNEL_MPLS_PW_MODE_TAGGED == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( True == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_MPLS_PW_WITH_CW = %g ###"  %attribute.value.booldata)
                assert ( True == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_ESI_LABEL_VALID = %g ###"  %attribute.value.booldata)
                assert ( False == attribute.value.booldata )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID:
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_NEXTHOP_ID = %x ###"  %attribute.value.oid)
                assert ( nhop_pw_pe1_to_pe4 == attribute.value.oid )
        warmboot(self.client)
        sys_logging("9.Send encap and decap traffic packet,check packet receiving")
        dmac_P = provider_nexthop_mac_list[0]
        sys_logging("9.1 Encap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_remote_list[num],
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_remote_list[num],
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            nhp_pw_label = nhp_pw_label_list[num]
            mpls_label_stack = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw_label,'tc':0,'ttl':64,'s':1},{'label':0,'tc':0,'ttl':0,'s':0}]
            pw_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(1, str(uni_pkt))
            self.ctc_verify_packet(pw_pkt, 0)
        sys_logging("9.2 Decap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            inseg_pw_label = inseg_pw_label_list[num]
            mpls_label_stack = [{'label':inseg_lsp_label,'tc':0,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':64,'s':1},{'label':0,'tc':0,'ttl':0,'s':0}]
            pw_pkt = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(0, str(pw_pkt))
            self.ctc_verify_packet(uni_pkt, 1)    
        sys_logging("10.Clear configuration")
        for num in range(2):
            uni_port_oid = uni_port_oid_list[num]
            mac_host_local = mac_host_local_list[num]
            sai_thrift_delete_fdb(self.client, bridge_id, mac_host_local, uni_port_oid)
        for num in range(3):
            mac_host_remote = mac_host_remote_list[num]
            bport_pw_oid = bport_pw_oid_list[num]
            sai_thrift_delete_fdb(self.client, bridge_id, mac_host_remote, bport_pw_oid)
        for num in range(2):
            uni_port=port_list[num+1]
            uni_port_oid = uni_port_oid_list[num]
            sai_thrift_remove_bridge_sub_port(self.client, uni_port_oid, uni_port)
        self.client.sai_thrift_remove_bridge_port(pw4_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw4_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe4)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw4)
        self.client.sai_thrift_remove_router_interface(pw4_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw3_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe3)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw3)
        self.client.sai_thrift_remove_router_interface(pw3_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw2_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe2)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw2)
        self.client.sai_thrift_remove_router_interface(pw2_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge(bridge_id)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_lsp_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_lsp_pe1_to_p)
        self.client.sai_thrift_remove_router_interface(mpls_rif_oid)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            provider_nhop_oid = provider_nhop_oid_list[num]
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            self.client.sai_thrift_remove_next_hop(provider_nhop_oid)
            sai_thrift_delete_fdb(self.client, vlan_oid, nexthop_mac, tmp_port)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
            sai_thrift_remove_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            self.client.sai_thrift_remove_router_interface(provider_rif_oid)
        for num in range(1):
            vlan_oid = provider_vlan_oid_list[num]
            vlan_id = provider_vlan_list[num]
            sai_thrift_vlan_remove_all_ports(self.client, vlan_oid)
            self.client.sai_thrift_remove_vlan(vlan_oid)

class Scenario002_MplsVpnTestVpws(sai_base_test.ThriftInterfaceDataPlane):
    def runTest(self):
        """
        1.Set provider routing configuration
        2.Set provider mpls lsp configuration
        3.Set provider mpls pw configuration
        4.Set Provider mpls uni port configuration
        5.Bind VPWS PW and AC port
        9.Send encap and decap traffic packet,check packet receiving
        10.Clear configuration
        11.Repeat 1-10 for 3 times
        """
        sys_logging("1.Set provider routing configuration")
        switch_init(self.client)
        provider_vlan_list = [100]
        provider_vlan_oid_list = [0 for i in range(1)]
        provider_vlan_member_oid_list = [0 for i in range(1)]
        for num in range(1):
            vlan_id = provider_vlan_list[num]
            vlan_oid = sai_thrift_create_vlan(self.client, vlan_id)
            provider_vlan_oid_list[num] = vlan_oid
            tmp_port = port_list[num]
            vlan_member = sai_thrift_create_vlan_member(self.client, vlan_oid, tmp_port, SAI_VLAN_TAGGING_MODE_TAGGED)
            provider_vlan_member_oid_list[num] = vlan_member
        v4_enabled = 1
        v6_enabled = 1
        default_vrf_oid = 0x0000000003
        mac=router_mac
        provider_rif_oid_list = [0 for i in range(1)]
        for num in range(1):
            tmp_vlan_oid = provider_vlan_oid_list[num]
            provider_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_VLAN, 0, tmp_vlan_oid, v4_enabled, v6_enabled, mac)
            provider_rif_oid_list[num] = provider_rif_oid
        addr_family = SAI_IP_ADDR_FAMILY_IPV4
        ip_mask_32 = '255.255.255.255'
        ip_mask_24 = '255.255.255.0'
        provider_rif_ip_addr_list = ['192.168.1.1']
        provider_nexthop_ip_addr_list = ['192.168.1.2']
        provider_nexthop_mac_list = ['00:11:22:33:44:01']
        provider_nhop_oid_list = [0 for i in range(1)]
        mac_action = SAI_PACKET_ACTION_FORWARD
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0, packet_action=SAI_PACKET_ACTION_TRAP)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            ret = sai_thrift_create_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_neighbor failed!!! ret = %d" % ret)
            provider_nhop_oid = sai_thrift_create_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid)
            provider_nhop_oid_list[num] = provider_nhop_oid
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            sai_thrift_create_fdb(self.client, vlan_oid, nexthop_mac, tmp_port, mac_action)
        sys_logging("2.Set provider mpls lsp configuration")
        mpls_if_mac = ''
        mpls_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        nhp_lsp_label = 200
        nhp_lsp_label_for_nhp = (nhp_lsp_label<<12) | 64
        nhp_lsp_label_list = [nhp_lsp_label_for_nhp]
        nexthop_ip_addr = provider_nexthop_ip_addr_list[0]
        provider_rif_oid = provider_rif_oid_list[0]
        
        nhop_lsp_pe1_to_p = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, nhp_lsp_label_list)
        
        inseg_lsp_label = 100
        pop_nums = 1
        inseg_nhop_lsp = mpls_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_lsp_label, pop_nums, None, inseg_nhop_lsp, packet_action)
        
        sys_logging("3.Set provider mpls pw configuration")
        
        bridge_id = sai_thrift_create_bridge(self.client, SAI_BRIDGE_TYPE_CROSS_CONNECT)
        sys_logging(">>bridge_id = %d" % bridge_id)
        
        pw2_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw2_label = 102
        nhp_pw2_label = 202
        nhp_pw2_label_for_list = (nhp_pw2_label<<12) | 64
        
        label_list = [nhp_pw2_label_for_list]
        
        tunnel_id_pw2 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=True, encap_cw_en=True, encap_ttl_val=0, encap_tagged_vlan=200)
        nhop_pw_pe1_to_pe2 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw2, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw2 = pw2_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_pw2_label, pop_nums, None, inseg_nhop_pw2, packet_action, tunnel_id=tunnel_id_pw2)
        
        pw2_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw2, bridge_id=bridge_id, admin_state=False)
        
        pw3_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw3_label = 103
        nhp_pw3_label = 202
        nhp_pw3_label_for_list = (nhp_pw3_label<<12) | 64
        label_list = [nhp_pw3_label_for_list]
        tunnel_id_pw3 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=True, encap_cw_en=True, encap_ttl_val=0, encap_tagged_vlan=300)
        nhop_pw_pe1_to_pe3 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw3, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw3 = pw3_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw3_label, pop_nums, None, inseg_nhop_pw3, packet_action, tunnel_id=tunnel_id_pw3)
        pw3_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw3, bridge_id=bridge_id, admin_state=False)
        
        bport_pw_oid_list=[pw2_tunnel_bport_oid, pw3_tunnel_bport_oid]
        nhp_pw_label_list=[nhp_pw2_label, nhp_pw3_label]
        inseg_pw_label_list=[inseg_pw2_label,inseg_pw3_label]
        sys_logging("4.Set Provider mpls uni port configuration")
        uni_vlan_id_list = [1001,1002]
        uni_port_oid_list=[0 for i in range(2)]
        tagged_vlan_list=[200,300]
        for num in range(2):
            uni_port=port_list[num+1]
            uni_vlan_id = uni_vlan_id_list[num]
            uni_port_oid = sai_thrift_create_bridge_sub_port(self.client, uni_port, bridge_id, uni_vlan_id, False)
            uni_port_oid_list[num] = uni_port_oid
        sys_logging("5.Bind VPWS PW and AC port")
        mac_host_remote_list = ['00:88:88:99:01:01','00:88:88:99:02:01']
        mac_host_local_list = ['00:88:88:88:01:01','00:88:88:88:02:01']
        ip_host_remote_list = ['1.1.1.102','1.1.1.103']
        ip_host_local_list = ['1.1.1.1','1.1.1.2']
        mac_action = SAI_PACKET_ACTION_FORWARD
        
        uni_port_oid = uni_port_oid_list[0]
        bport_attr_xcport_value = sai_thrift_attribute_value_t(oid=pw2_tunnel_bport_oid)
        bport_attr_xcport = sai_thrift_attribute_t(id=SAI_BRIDGE_PORT_ATTR_CROSS_CONNECT_BRIDGE_PORT,
                                                    value=bport_attr_xcport_value)
        
        self.client.sai_thrift_set_bridge_port_attribute(uni_port_oid, bport_attr_xcport)
        
        bport_attr_xcport_value = sai_thrift_attribute_value_t(oid=uni_port_oid)
        bport_attr_xcport = sai_thrift_attribute_t(id=SAI_BRIDGE_PORT_ATTR_CROSS_CONNECT_BRIDGE_PORT,
                                                    value=bport_attr_xcport_value)
        self.client.sai_thrift_set_bridge_port_attribute(pw2_tunnel_bport_oid, bport_attr_xcport)
        
        bport_attr_xcport_value = sai_thrift_attribute_value_t(booldata=True)
        bport_attr_xcport = sai_thrift_attribute_t(id=SAI_BRIDGE_PORT_ATTR_ADMIN_STATE,
                                                    value=bport_attr_xcport_value)
        self.client.sai_thrift_set_bridge_port_attribute(uni_port_oid, bport_attr_xcport)
        self.client.sai_thrift_set_bridge_port_attribute(pw2_tunnel_bport_oid, bport_attr_xcport)
        
        uni_port_oid = uni_port_oid_list[1]
        bport_attr_xcport_value = sai_thrift_attribute_value_t(oid=pw3_tunnel_bport_oid)
        bport_attr_xcport = sai_thrift_attribute_t(id=SAI_BRIDGE_PORT_ATTR_CROSS_CONNECT_BRIDGE_PORT,
                                                    value=bport_attr_xcport_value)
        self.client.sai_thrift_set_bridge_port_attribute(uni_port_oid, bport_attr_xcport)
        
        bport_attr_xcport_value = sai_thrift_attribute_value_t(oid=uni_port_oid)
        bport_attr_xcport = sai_thrift_attribute_t(id=SAI_BRIDGE_PORT_ATTR_CROSS_CONNECT_BRIDGE_PORT,
                                                    value=bport_attr_xcport_value)
        self.client.sai_thrift_set_bridge_port_attribute(pw3_tunnel_bport_oid, bport_attr_xcport)
        
        bport_attr_xcport_value = sai_thrift_attribute_value_t(booldata=True)
        bport_attr_xcport = sai_thrift_attribute_t(id=SAI_BRIDGE_PORT_ATTR_ADMIN_STATE,
                                                    value=bport_attr_xcport_value)
        self.client.sai_thrift_set_bridge_port_attribute(uni_port_oid, bport_attr_xcport)
        self.client.sai_thrift_set_bridge_port_attribute(pw3_tunnel_bport_oid, bport_attr_xcport)
        
        warmboot(self.client)
        sys_logging("9.Send encap and decap traffic packet,check packet receiving")
        dmac_P = provider_nexthop_mac_list[0]
        sys_logging("9.1 Encap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_remote_list[num],
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_remote_list[num],
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            nhp_pw_label = nhp_pw_label_list[num]
            mpls_label_stack = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw_label,'tc':0,'ttl':64,'s':1},{'label':0,'tc':0,'ttl':0,'s':0}]
            pw_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(1, str(uni_pkt))
            self.ctc_verify_packet(pw_pkt, 0)
        sys_logging("9.2 Decap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            inseg_pw_label = inseg_pw_label_list[num]
            mpls_label_stack = [{'label':inseg_lsp_label,'tc':0,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':64,'s':1},{'label':0,'tc':0,'ttl':0,'s':0}]
            pw_pkt = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(0, str(pw_pkt))
            self.ctc_verify_packet(uni_pkt, 1)    
        sys_logging("10.Clear configuration")
        for num in range(2):
            uni_port=port_list[num+1]
            uni_port_oid = uni_port_oid_list[num]
            sai_thrift_remove_bridge_sub_port(self.client, uni_port_oid, uni_port)
        self.client.sai_thrift_remove_bridge_port(pw3_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe3)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw3)
        self.client.sai_thrift_remove_router_interface(pw3_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw2_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe2)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw2)
        self.client.sai_thrift_remove_router_interface(pw2_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge(bridge_id)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_lsp_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_lsp_pe1_to_p)
        self.client.sai_thrift_remove_router_interface(mpls_rif_oid)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            provider_nhop_oid = provider_nhop_oid_list[num]
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            self.client.sai_thrift_remove_next_hop(provider_nhop_oid)
            sai_thrift_delete_fdb(self.client, vlan_oid, nexthop_mac, tmp_port)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
            sai_thrift_remove_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            self.client.sai_thrift_remove_router_interface(provider_rif_oid)
        for num in range(1):
            vlan_oid = provider_vlan_oid_list[num]
            vlan_id = provider_vlan_list[num]
            sai_thrift_vlan_remove_all_ports(self.client, vlan_oid)
            self.client.sai_thrift_remove_vlan(vlan_oid)

class Scenario003_MplsVpnTestL3vpn(sai_base_test.ThriftInterfaceDataPlane):
    def runTest(self):
        """
        1.Set provider routing configuration
        2.Set provider mpls lsp configuration
        3.Set provider mpls pw configuration
        4.Set Provider mpls uni interface configuration
        5.Set FIB entry
        9.Send encap and decap traffic packet,check packet receiving
        10.Clear configuration
        11.Repeat 1-10 for 3 times
        """
        sys_logging("1.Set provider routing configuration")
        switch_init(self.client)
        provider_vlan_list = [100]
        provider_vlan_oid_list = [0 for i in range(1)]
        provider_vlan_member_oid_list = [0 for i in range(1)]
        for num in range(1):
            vlan_id = provider_vlan_list[num]
            vlan_oid = sai_thrift_create_vlan(self.client, vlan_id)
            provider_vlan_oid_list[num] = vlan_oid
            tmp_port = port_list[num]
            vlan_member = sai_thrift_create_vlan_member(self.client, vlan_oid, tmp_port, SAI_VLAN_TAGGING_MODE_TAGGED)
            provider_vlan_member_oid_list[num] = vlan_member
        v4_enabled = 1
        v6_enabled = 1
        default_vrf_oid = 0x0000000003
        mac=router_mac
        provider_rif_oid_list = [0 for i in range(1)]
        for num in range(1):
            tmp_vlan_oid = provider_vlan_oid_list[num]
            provider_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_VLAN, 0, tmp_vlan_oid, v4_enabled, v6_enabled, mac)
            provider_rif_oid_list[num] = provider_rif_oid
        addr_family = SAI_IP_ADDR_FAMILY_IPV4
        ip_mask_32 = '255.255.255.255'
        ip_mask_24 = '255.255.255.0'
        provider_rif_ip_addr_list = ['192.168.1.1']
        provider_nexthop_ip_addr_list = ['192.168.1.2']
        provider_nexthop_mac_list = ['00:11:22:33:44:01']
        provider_nhop_oid_list = [0 for i in range(1)]
        mac_action = SAI_PACKET_ACTION_FORWARD
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0, packet_action=SAI_PACKET_ACTION_TRAP)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            ret = sai_thrift_create_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_neighbor failed!!! ret = %d" % ret)
            provider_nhop_oid = sai_thrift_create_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid)
            provider_nhop_oid_list[num] = provider_nhop_oid
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            sai_thrift_create_fdb(self.client, vlan_oid, nexthop_mac, tmp_port, mac_action)
        sys_logging("2.Set provider mpls lsp configuration")
        mpls_if_mac = ''
        mpls_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        nhp_lsp_label = 200
        nhp_lsp_label_for_nhp = (nhp_lsp_label<<12) | 64
        nhp_lsp_label_list = [nhp_lsp_label_for_nhp]
        nexthop_ip_addr = provider_nexthop_ip_addr_list[0]
        provider_rif_oid = provider_rif_oid_list[0]
        nhop_lsp_pe1_to_p = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, nhp_lsp_label_list)
        
        inseg_lsp_label = 100
        pop_nums = 1
        inseg_nhop_lsp = mpls_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_lsp_label, pop_nums, None, inseg_nhop_lsp, packet_action)
        
        sys_logging("3.Set provider mpls pw configuration")
        pw2_vr_id = sai_thrift_create_virtual_router(self.client, v4_enabled, v6_enabled)
        pw3_vr_id = sai_thrift_create_virtual_router(self.client, v4_enabled, v6_enabled)
        pw2_rif_oid = sai_thrift_create_router_interface(self.client, pw2_vr_id, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        pw3_rif_oid = sai_thrift_create_router_interface(self.client, pw3_vr_id, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        inseg_pw2_label = 102
        nhp_pw2_label = 202
        nhp_pw2_label_for_list = (nhp_pw2_label<<12) | 64
        label_list = [nhp_pw2_label_for_list]
        tunnel_id_pw2 = sai_thrift_create_tunnel_mpls(self.client)

        nhop_pw_pe1_to_pe2 = sai_thrift_create_mpls_nhop(self.client, addr_family, '', None, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p, tunnel_id=tunnel_id_pw2)
        pop_nums = 1
        inseg_nhop_pw2 = pw2_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw2_label, pop_nums, None, inseg_nhop_pw2, packet_action, tunnel_id=tunnel_id_pw2)
        
        inseg_pw3_label = 103
        nhp_pw3_label = 203
        nhp_pw3_label_for_list = (nhp_pw3_label<<12) | 64
        label_list = [nhp_pw3_label_for_list]
        tunnel_id_pw3 = sai_thrift_create_tunnel_mpls(self.client)

        nhop_pw_pe1_to_pe3 = sai_thrift_create_mpls_nhop(self.client, addr_family, '', None, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p, tunnel_id=tunnel_id_pw3)
        pop_nums = 1
        inseg_nhop_pw3 = pw3_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw3_label, pop_nums, None, inseg_nhop_pw3, packet_action, tunnel_id=tunnel_id_pw3)

        nhp_pw_label_list=[nhp_pw2_label, nhp_pw3_label]
        inseg_pw_label_list=[inseg_pw2_label,inseg_pw3_label]
        sys_logging("4.Set Provider mpls uni interface configuration")
        uni_vlan_id_list = [1001,1002]
        tagged_vlan_list=[200,300]
        vr_list=[pw2_vr_id,pw3_vr_id]
        rif_uni_oid_list=[[0 for i in range(2)] for i in range(2)]
        for num in range(2):
            uni_port=port_list[num+1]
            for num2 in range(2):
                vr_id = vr_list[num2]
                uni_vlan_id = uni_vlan_id_list[num2]
                rif_uni_oid = sai_thrift_create_router_interface(self.client, vr_id, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT, uni_port, None, v4_enabled, v6_enabled, '', outer_vlan_id=uni_vlan_id)
                rif_uni_oid_list[num][num2] = rif_uni_oid
                
        sys_logging("5.Set FIB entry")
        mac_uni_peer_list = [['00:88:88:99:01:01','00:88:88:99:02:01'],['00:88:88:77:01:01','00:88:88:77:02:01']]
        mac_host_local_list = ['00:88:88:88:01:01','00:88:88:88:02:01']
        pw_side_ip_list = ['1.1.3.2','1.1.1.2']
        uni_side_ip_list = [['1.1.1.2','1.1.2.2'], ['1.1.2.2','1.1.3.2']]
        mac_action = SAI_PACKET_ACTION_FORWARD
        uni_nhop_list = [[0 for i in range(2)] for i in range(2)]
        for num in range(2):
            for num2 in range(2):
                vr_id = vr_list[num2]
                rif_uni_oid = rif_uni_oid_list[num][num2]
                uni_side_ip = uni_side_ip_list[num][num2]
                mac_uni_peer = mac_uni_peer_list[num][num2]
                sai_thrift_create_neighbor(self.client, addr_family, rif_uni_oid, uni_side_ip, mac_uni_peer)
                uni_nhop = sai_thrift_create_nhop(self.client, addr_family, uni_side_ip, rif_uni_oid)
                ret = sai_thrift_create_route(self.client, vr_id, addr_family, uni_side_ip, ip_mask_24, uni_nhop)
                uni_nhop_list[num][num2] = uni_nhop
        nhop_pw_list = [nhop_pw_pe1_to_pe2,nhop_pw_pe1_to_pe3]
        for num in range(2):
            vr_id = vr_list[num]
            pw_side_ip = pw_side_ip_list[num]
            nhop_pw = nhop_pw_list[num]
            ret = sai_thrift_create_route(self.client, vr_id, addr_family, pw_side_ip, ip_mask_24, nhop_pw)
        sys_logging("8.Get and Set attribute")
        ids_list = [SAI_TUNNEL_ATTR_DECAP_TTL_MODE,SAI_TUNNEL_ATTR_ENCAP_TTL_MODE,SAI_TUNNEL_ATTR_ENCAP_TTL_VAL,SAI_TUNNEL_ATTR_DECAP_EXP_MODE,SAI_TUNNEL_ATTR_ENCAP_EXP_MODE,SAI_TUNNEL_ATTR_ENCAP_EXP_VAL]
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw2,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_TTL_MODE_UNIFORM_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_TTL_MODE_UNIFORM_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_VAL:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_VAL = %d ###"  % u8.value)
                assert ( 0 == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_EXP_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_EXP_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_EXP_MODE_UNIFORM_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_EXP_MODE_UNIFORM_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_EXP_VAL:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_EXP_VAL = %d ###"  % u8.value)
                assert ( 0 == u8.value )
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw3,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_TTL_MODE_UNIFORM_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_TTL_MODE_UNIFORM_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_VAL:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_VAL = %d ###"  % u8.value)
                assert ( 0 == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_EXP_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_EXP_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_EXP_MODE_UNIFORM_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_EXP_MODE_UNIFORM_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_EXP_VAL:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_EXP_VAL = %d ###"  % u8.value)
                assert ( 0 == u8.value )
        warmboot(self.client)
        sys_logging("9.Send encap and decap traffic packet,check packet receiving")
        dmac_P = provider_nexthop_mac_list[0]
        sys_logging("9.1 Encap")
        for num in range(1):
            for num2 in range(1):
                uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=router_mac,
                                eth_src=mac_uni_peer_list[num][num2],
                                dl_vlan_enable=True,
                                vlan_vid=uni_vlan_id_list[num2],
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=pw_side_ip_list[num2],
                                ip_src=uni_side_ip_list[num][num2],
                                ip_id=105,
                                ip_ttl=40,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                mpls_inner_pkt = simple_ip_only_packet(pktlen=78,
                                ip_src=uni_side_ip_list[num][num2],
                                ip_dst=pw_side_ip_list[num2],
                                ip_ttl=39,
                                ip_id=105,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                nhp_pw_label = nhp_pw_label_list[num2]
                mpls_label_stack = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw_label,'tc':0,'ttl':39,'s':1}]
                pw_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
                self.ctc_send_packet(1, str(uni_pkt))
                self.ctc_verify_packet(pw_pkt, 0)
        sys_logging("9.2 Decap")
        for num in range(1):
            for num2 in range(1):
                uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_uni_peer_list[num][num2],
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=uni_vlan_id_list[num2],
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=uni_side_ip_list[num][num2],
                                ip_src=pw_side_ip_list[num2],
                                ip_id=105,
                                ip_ttl=79,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                mpls_inner_pkt = simple_ip_only_packet(pktlen=78,
                                ip_src=pw_side_ip_list[num2],
                                ip_dst=uni_side_ip_list[num][num2],
                                ip_ttl=64,
                                ip_id=105,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                inseg_pw_label = inseg_pw_label_list[num2]
                mpls_label_stack = [{'label':inseg_lsp_label,'tc':0,'ttl':80,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':40,'s':1}]
                pw_pkt = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
                self.ctc_send_packet(0, str(pw_pkt))
                self.ctc_verify_packet(uni_pkt, 1)    
        sys_logging("10.Clear configuration")
        for num in range(2):
            vr_id = vr_list[num]
            pw_side_ip = pw_side_ip_list[num]
            nhop_pw = nhop_pw_list[num]
            ret = sai_thrift_remove_route(self.client, vr_id, addr_family, pw_side_ip, ip_mask_24, nhop_pw)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(2):
            for num2 in range(2):
                vr_id = vr_list[num2]
                rif_uni_oid = rif_uni_oid_list[num][num2]
                uni_side_ip = uni_side_ip_list[num][num2]
                uni_nhop = uni_nhop_list[num][num2]
                mac_uni_peer = mac_uni_peer_list[num][num2]
                sai_thrift_remove_neighbor(self.client, addr_family, rif_uni_oid, uni_side_ip, mac_uni_peer)
                ret = sai_thrift_remove_route(self.client, vr_id, addr_family, uni_side_ip, ip_mask_24, uni_nhop)
                if (ret < 0):
                    sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
                self.client.sai_thrift_remove_next_hop(uni_nhop)
        for num in range(2):
            for num2 in range(2):
                rif_uni_oid = rif_uni_oid_list[num][num2]
                self.client.sai_thrift_remove_router_interface(rif_uni_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe3)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw3)
        
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe2)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw2)
        
        self.client.sai_thrift_remove_router_interface(pw3_rif_oid)
        self.client.sai_thrift_remove_router_interface(pw2_rif_oid)
        
        self.client.sai_thrift_remove_virtual_router(pw3_vr_id)
        self.client.sai_thrift_remove_virtual_router(pw2_vr_id)
        
        inseg_entry = sai_thrift_inseg_entry_t(inseg_lsp_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_lsp_pe1_to_p)
        self.client.sai_thrift_remove_router_interface(mpls_rif_oid)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            provider_nhop_oid = provider_nhop_oid_list[num]
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            self.client.sai_thrift_remove_next_hop(provider_nhop_oid)
            sai_thrift_delete_fdb(self.client, vlan_oid, nexthop_mac, tmp_port)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
            sai_thrift_remove_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            self.client.sai_thrift_remove_router_interface(provider_rif_oid)
        for num in range(1):
            vlan_oid = provider_vlan_oid_list[num]
            vlan_id = provider_vlan_list[num]
            sai_thrift_vlan_remove_all_ports(self.client, vlan_oid)
            self.client.sai_thrift_remove_vlan(vlan_oid)

class Scenario004_MplsVpnTestL3VpnSr(sai_base_test.ThriftInterfaceDataPlane):
    def runTest(self):
        """
        1.Set provider routing configuration
        2.Set provider mpls sr configuration
        3.Set provider mpls pw configuration
        4.Set Provider mpls uni interface configuration
        5.Set FIB entry
        9.Send encap and decap traffic packet,check packet receiving
        10.Clear configuration
        11.Repeat 1-10 for 3 times
        """
        sys_logging("1.Set provider routing configuration")
        switch_init(self.client)
        provider_vlan_list = [100]
        provider_vlan_oid_list = [0 for i in range(1)]
        provider_vlan_member_oid_list = [0 for i in range(1)]
        for num in range(1):
            vlan_id = provider_vlan_list[num]
            vlan_oid = sai_thrift_create_vlan(self.client, vlan_id)
            provider_vlan_oid_list[num] = vlan_oid
            tmp_port = port_list[num]
            vlan_member = sai_thrift_create_vlan_member(self.client, vlan_oid, tmp_port, SAI_VLAN_TAGGING_MODE_TAGGED)
            provider_vlan_member_oid_list[num] = vlan_member
        v4_enabled = 1
        v6_enabled = 1
        default_vrf_oid = 0x0000000003
        mac=router_mac
        provider_rif_oid_list = [0 for i in range(1)]
        for num in range(1):
            tmp_vlan_oid = provider_vlan_oid_list[num]
            provider_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_VLAN, 0, tmp_vlan_oid, v4_enabled, v6_enabled, mac)
            provider_rif_oid_list[num] = provider_rif_oid
        addr_family = SAI_IP_ADDR_FAMILY_IPV4
        ip_mask_32 = '255.255.255.255'
        ip_mask_24 = '255.255.255.0'
        provider_rif_ip_addr_list = ['192.168.1.1']
        provider_nexthop_ip_addr_list = ['192.168.1.2']
        provider_nexthop_mac_list = ['00:11:22:33:44:01']
        provider_nhop_oid_list = [0 for i in range(1)]
        mac_action = SAI_PACKET_ACTION_FORWARD
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0, packet_action=SAI_PACKET_ACTION_TRAP)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            ret = sai_thrift_create_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_neighbor failed!!! ret = %d" % ret)
            provider_nhop_oid = sai_thrift_create_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid)
            provider_nhop_oid_list[num] = provider_nhop_oid
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            sai_thrift_create_fdb(self.client, vlan_oid, nexthop_mac, tmp_port, mac_action)
        sys_logging("2.Set provider mpls sr configuration")
        mpls_if_mac = ''
        mpls_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        nhp_lsp_label = 200
        nhp_lsp_label_for_nhp = (nhp_lsp_label<<12) | 64
        nhp_lsp_label_list = [nhp_lsp_label_for_nhp]
        nexthop_ip_addr = provider_nexthop_ip_addr_list[0]
        provider_rif_oid = provider_rif_oid_list[0]
        nhop_lsp_pe1_to_p = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, nhp_lsp_label_list)
        
        inseg_lsp_label = 100
        pop_nums = 1
        inseg_nhop_lsp = mpls_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_lsp_label, pop_nums, None, inseg_nhop_lsp, packet_action)
                
        inseg_sr2_label = 102
        nhp_sr2_label_1 = 302
        nhp_sr2_label_2 = 402
        nhp_sr2_label_3 = 502
        nhp_sr2_label_4 = 602
        nhp_sr2_label_5 = 702
        
        nhp_sr2_label_1_for_list = (nhp_sr2_label_1<<12) | 64
        nhp_sr2_label_2_for_list = (nhp_sr2_label_2<<12) | 64
        nhp_sr2_label_3_for_list = (nhp_sr2_label_3<<12) | 64
        nhp_sr2_label_4_for_list = (nhp_sr2_label_4<<12) | 64
        nhp_sr2_label_5_for_list = (nhp_sr2_label_5<<12) | 64
        nhp_sr2_label_6_for_list = (nhp_lsp_label<<12) | 64
        
        label_list = [nhp_sr2_label_1_for_list,nhp_sr2_label_2_for_list,nhp_sr2_label_3_for_list,nhp_sr2_label_4_for_list,nhp_sr2_label_5_for_list,nhp_sr2_label_6_for_list]

        nhop_sr_pe1_to_pe2 = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, label_list)
        pop_nums = 1
        inseg_nhop_sr2 = provider_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_sr2_label, pop_nums, None, inseg_nhop_sr2, packet_action)
        
        inseg_sr3_label = 103
        nhp_sr3_label_1 = 303
        nhp_sr3_label_2 = 403
        nhp_sr3_label_3 = 503
        nhp_sr3_label_4 = 603
        nhp_sr3_label_5 = 703
        
        nhp_sr3_label_1_for_list = (nhp_sr3_label_1<<12) | 64
        nhp_sr3_label_2_for_list = (nhp_sr3_label_2<<12) | 64
        nhp_sr3_label_3_for_list = (nhp_sr3_label_3<<12) | 64
        nhp_sr3_label_4_for_list = (nhp_sr3_label_4<<12) | 64
        nhp_sr3_label_5_for_list = (nhp_sr3_label_5<<12) | 64
        nhp_sr3_label_6_for_list = (nhp_lsp_label<<12) | 64
        
        label_list = [nhp_sr3_label_1_for_list,nhp_sr3_label_2_for_list,nhp_sr3_label_3_for_list,nhp_sr3_label_4_for_list,nhp_sr3_label_5_for_list,nhp_sr3_label_6_for_list]

        nhop_sr_pe1_to_pe3 = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, label_list)
        pop_nums = 1
        inseg_nhop_sr3 = provider_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_sr3_label, pop_nums, None, inseg_nhop_sr3, packet_action)

        nhp_sr_label_list=[[nhp_sr2_label_1,nhp_sr2_label_2,nhp_sr2_label_3,nhp_sr2_label_4,nhp_sr2_label_5,nhp_lsp_label], [nhp_sr3_label_1,nhp_sr3_label_2,nhp_sr3_label_3,nhp_sr3_label_4,nhp_sr3_label_5,nhp_lsp_label]]
        inseg_sr_label_list=[inseg_sr2_label,inseg_sr3_label]
        
        sys_logging("3.Set provider mpls pw configuration")
        pw2_vr_id = sai_thrift_create_virtual_router(self.client, v4_enabled, v6_enabled)
        pw3_vr_id = sai_thrift_create_virtual_router(self.client, v4_enabled, v6_enabled)
        pw2_rif_oid = sai_thrift_create_router_interface(self.client, pw2_vr_id, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        pw3_rif_oid = sai_thrift_create_router_interface(self.client, pw3_vr_id, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        inseg_pw2_label = 1002
        nhp_pw2_label = 202
        nhp_pw2_label_for_list = (nhp_pw2_label<<12) | 64
        label_list = [nhp_pw2_label_for_list]
        tunnel_id_pw2 = sai_thrift_create_tunnel_mpls(self.client)
        
        nhop_pw_pe1_to_pe2 = sai_thrift_create_mpls_nhop(self.client, addr_family, '', None, label_list, next_level_nhop_oid=nhop_sr_pe1_to_pe2, tunnel_id=tunnel_id_pw2)
        pop_nums = 1
        inseg_nhop_pw2 = pw2_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw2_label, pop_nums, None, inseg_nhop_pw2, packet_action, tunnel_id=tunnel_id_pw2)
        
        inseg_pw3_label = 1003
        nhp_pw3_label = 203
        nhp_pw3_label_for_list = (nhp_pw3_label<<12) | 64
        label_list = [nhp_pw3_label_for_list]
        tunnel_id_pw3 = sai_thrift_create_tunnel_mpls(self.client)
        
        nhop_pw_pe1_to_pe3 = sai_thrift_create_mpls_nhop(self.client, addr_family, '', None, label_list, next_level_nhop_oid=nhop_sr_pe1_to_pe3, tunnel_id=tunnel_id_pw3)
        pop_nums = 1
        inseg_nhop_pw3 = pw3_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw3_label, pop_nums, None, inseg_nhop_pw3, packet_action, tunnel_id=tunnel_id_pw3)

        nhp_pw_label_list=[nhp_pw2_label, nhp_pw3_label]
        inseg_pw_label_list=[inseg_pw2_label,inseg_pw3_label]
        
        sys_logging("4.Set Provider mpls uni interface configuration")
        uni_vlan_id_list = [1001,1002]
        tagged_vlan_list=[200,300]
        vr_list=[pw2_vr_id,pw3_vr_id]
        rif_uni_oid_list=[[0 for i in range(2)] for i in range(2)]
        for num in range(2):
            uni_port=port_list[num+1]
            for num2 in range(2):
                vr_id = vr_list[num2]
                uni_vlan_id = uni_vlan_id_list[num2]
                rif_uni_oid = sai_thrift_create_router_interface(self.client, vr_id, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT, uni_port, None, v4_enabled, v6_enabled, '', outer_vlan_id=uni_vlan_id)
                rif_uni_oid_list[num][num2] = rif_uni_oid
                
        sys_logging("5.Set FIB entry")
        mac_uni_peer_list = [['00:88:88:99:01:01','00:88:88:99:02:01'],['00:88:88:77:01:01','00:88:88:77:02:01']]
        mac_host_local_list = ['00:88:88:88:01:01','00:88:88:88:02:01']
        pw_side_ip_list = ['1.1.3.2','1.1.1.2']
        uni_side_ip_list = [['1.1.1.2','1.1.2.2'], ['1.1.2.2','1.1.3.2']]
        mac_action = SAI_PACKET_ACTION_FORWARD
        uni_nhop_list = [[0 for i in range(2)] for i in range(2)]
        for num in range(2):
            for num2 in range(2):
                vr_id = vr_list[num2]
                rif_uni_oid = rif_uni_oid_list[num][num2]
                uni_side_ip = uni_side_ip_list[num][num2]
                mac_uni_peer = mac_uni_peer_list[num][num2]
                sai_thrift_create_neighbor(self.client, addr_family, rif_uni_oid, uni_side_ip, mac_uni_peer)
                uni_nhop = sai_thrift_create_nhop(self.client, addr_family, uni_side_ip, rif_uni_oid)
                ret = sai_thrift_create_route(self.client, vr_id, addr_family, uni_side_ip, ip_mask_24, uni_nhop)
                uni_nhop_list[num][num2] = uni_nhop
        nhop_pw_list = [nhop_pw_pe1_to_pe2,nhop_pw_pe1_to_pe3]
        for num in range(2):
            vr_id = vr_list[num]
            pw_side_ip = pw_side_ip_list[num]
            nhop_pw = nhop_pw_list[num]
            ret = sai_thrift_create_route(self.client, vr_id, addr_family, pw_side_ip, ip_mask_24, nhop_pw)
        warmboot(self.client)
        sys_logging("9.Send encap and decap traffic packet,check packet receiving")
        dmac_P = provider_nexthop_mac_list[0]
        sys_logging("9.1 Encap")
        for num in range(1):
            for num2 in range(1):
                uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=router_mac,
                                eth_src=mac_uni_peer_list[num][num2],
                                dl_vlan_enable=True,
                                vlan_vid=uni_vlan_id_list[num2],
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=pw_side_ip_list[num2],
                                ip_src=uni_side_ip_list[num][num2],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                mpls_inner_pkt = simple_ip_only_packet(pktlen=78,
                                ip_src=uni_side_ip_list[num][num2],
                                ip_dst=pw_side_ip_list[num2],
                                ip_ttl=63,
                                ip_id=105,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                nhp_pw_label = nhp_pw_label_list[num2]
                nhp_sr_label_1 = nhp_sr_label_list[num2][0]
                nhp_sr_label_2 = nhp_sr_label_list[num2][1]
                nhp_sr_label_3 = nhp_sr_label_list[num2][2]
                nhp_sr_label_4 = nhp_sr_label_list[num2][3]
                nhp_sr_label_5 = nhp_sr_label_list[num2][4]
                mpls_label_stack = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_sr_label_5,'tc':0,'ttl':64,'s':0},{'label':nhp_sr_label_4,'tc':0,'ttl':64,'s':0},{'label':nhp_sr_label_3,'tc':0,'ttl':64,'s':0},{'label':nhp_sr_label_2,'tc':0,'ttl':64,'s':0},{'label':nhp_sr_label_1,'tc':0,'ttl':64,'s':0},{'label':nhp_pw_label,'tc':0,'ttl':62,'s':1}]
                pw_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
                self.ctc_send_packet(1, str(uni_pkt))
                self.ctc_verify_packet(pw_pkt, 0)
        sys_logging("9.2 Decap")
        for num in range(1):
            for num2 in range(1):
                uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_uni_peer_list[num][num2],
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=uni_vlan_id_list[num2],
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=uni_side_ip_list[num][num2],
                                ip_src=pw_side_ip_list[num2],
                                ip_id=105,
                                ip_ttl=63,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                mpls_inner_pkt = simple_ip_only_packet(pktlen=78,
                                ip_src=pw_side_ip_list[num2],
                                ip_dst=uni_side_ip_list[num][num2],
                                ip_ttl=64,
                                ip_id=105,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                inseg_pw_label = inseg_pw_label_list[num2]
                #mpls_label_stack = [{'label':inseg_lsp_label,'tc':0,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':64,'s':1}]
                mpls_label_stack = [{'label':inseg_pw_label,'tc':0,'ttl':64,'s':1}]
                pw_pkt = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
                self.ctc_send_packet(0, str(pw_pkt))
                self.ctc_verify_packet(uni_pkt, 1) 
        sys_logging("10.Clear configuration")
        for num in range(2):
            vr_id = vr_list[num]
            pw_side_ip = pw_side_ip_list[num]
            nhop_pw = nhop_pw_list[num]
            ret = sai_thrift_remove_route(self.client, vr_id, addr_family, pw_side_ip, ip_mask_24, nhop_pw)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(2):
            for num2 in range(2):
                vr_id = vr_list[num2]
                rif_uni_oid = rif_uni_oid_list[num][num2]
                uni_side_ip = uni_side_ip_list[num][num2]
                uni_nhop = uni_nhop_list[num][num2]
                mac_uni_peer = mac_uni_peer_list[num][num2]
                sai_thrift_remove_neighbor(self.client, addr_family, rif_uni_oid, uni_side_ip, mac_uni_peer)
                ret = sai_thrift_remove_route(self.client, vr_id, addr_family, uni_side_ip, ip_mask_24, uni_nhop)
                if (ret < 0):
                    sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
                self.client.sai_thrift_remove_next_hop(uni_nhop)
        for num in range(2):
            for num2 in range(2):
                rif_uni_oid = rif_uni_oid_list[num][num2]
                self.client.sai_thrift_remove_router_interface(rif_uni_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe3)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw3)
        
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe2)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw2)
        
        self.client.sai_thrift_remove_router_interface(pw3_rif_oid)
        self.client.sai_thrift_remove_router_interface(pw2_rif_oid)
        
        self.client.sai_thrift_remove_virtual_router(pw3_vr_id)
        self.client.sai_thrift_remove_virtual_router(pw2_vr_id)
        
        inseg_entry = sai_thrift_inseg_entry_t(inseg_sr3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_sr_pe1_to_pe3)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_sr2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_sr_pe1_to_pe2)
        
        inseg_entry = sai_thrift_inseg_entry_t(inseg_lsp_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_lsp_pe1_to_p)
        self.client.sai_thrift_remove_router_interface(mpls_rif_oid)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            provider_nhop_oid = provider_nhop_oid_list[num]
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            self.client.sai_thrift_remove_next_hop(provider_nhop_oid)
            sai_thrift_delete_fdb(self.client, vlan_oid, nexthop_mac, tmp_port)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
            sai_thrift_remove_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            self.client.sai_thrift_remove_router_interface(provider_rif_oid)
        for num in range(1):
            vlan_oid = provider_vlan_oid_list[num]
            vlan_id = provider_vlan_list[num]
            sai_thrift_vlan_remove_all_ports(self.client, vlan_oid)
            self.client.sai_thrift_remove_vlan(vlan_oid)
            
class Scenario005_MplsVpnTestVplsEvpnNonEs(sai_base_test.ThriftInterfaceDataPlane):
    def runTest(self):
        """
        1.Set provider routing configuration
        2.Set provider mpls lsp configuration
        3.Set provider mpls pw configuration
        4.Set Provider mpls uni port configuration
        5.Set FDB entry
        9.Send encap and decap traffic packet,check packet receiving
        10.Clear configuration
        11.Repeat 1-10 for 3 times
        """
        sys_logging("1.Set provider routing configuration")
        switch_init(self.client)
        provider_vlan_list = [100]
        provider_vlan_oid_list = [0 for i in range(1)]
        provider_vlan_member_oid_list = [0 for i in range(1)]
        for num in range(1):
            vlan_id = provider_vlan_list[num]
            vlan_oid = sai_thrift_create_vlan(self.client, vlan_id)
            provider_vlan_oid_list[num] = vlan_oid
            tmp_port = port_list[num]
            vlan_member = sai_thrift_create_vlan_member(self.client, vlan_oid, tmp_port, SAI_VLAN_TAGGING_MODE_TAGGED)
            provider_vlan_member_oid_list[num] = vlan_member
        v4_enabled = 1
        v6_enabled = 1
        default_vrf_oid = 0x0000000003
        mac=router_mac
        provider_rif_oid_list = [0 for i in range(1)]
        for num in range(1):
            tmp_vlan_oid = provider_vlan_oid_list[num]
            provider_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_VLAN, 0, tmp_vlan_oid, v4_enabled, v6_enabled, mac)
            provider_rif_oid_list[num] = provider_rif_oid
        addr_family = SAI_IP_ADDR_FAMILY_IPV4
        ip_mask_32 = '255.255.255.255'
        ip_mask_24 = '255.255.255.0'
        provider_rif_ip_addr_list = ['192.168.1.1']
        provider_nexthop_ip_addr_list = ['192.168.1.2']
        provider_nexthop_mac_list = ['00:11:22:33:44:01']
        provider_nhop_oid_list = [0 for i in range(1)]
        mac_action = SAI_PACKET_ACTION_FORWARD
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0, packet_action=SAI_PACKET_ACTION_TRAP)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            ret = sai_thrift_create_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_neighbor failed!!! ret = %d" % ret)
            provider_nhop_oid = sai_thrift_create_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid)
            provider_nhop_oid_list[num] = provider_nhop_oid
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            sai_thrift_create_fdb(self.client, vlan_oid, nexthop_mac, tmp_port, mac_action)
        sys_logging("2.Set provider mpls lsp configuration")
        mpls_if_mac = ''
        mpls_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        nhp_lsp_label = 200
        nhp_lsp_label_for_nhp = (nhp_lsp_label<<12) | 64
        nhp_lsp_label_list = [nhp_lsp_label_for_nhp]
        nexthop_ip_addr = provider_nexthop_ip_addr_list[0]
        provider_rif_oid = provider_rif_oid_list[0]
        
        nhop_lsp_pe1_to_p = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, nhp_lsp_label_list)
        
        inseg_lsp_label = 100
        pop_nums = 1
        inseg_nhop_lsp = mpls_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_lsp_label, pop_nums, None, inseg_nhop_lsp, packet_action)
        
        sys_logging("3.Set provider mpls pw configuration")
        bridge_id = sai_thrift_create_bridge(self.client, SAI_BRIDGE_TYPE_1D)
        sys_logging(">>bridge_id = %d" % bridge_id)
        
        pw2_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw2_label = 102
        nhp_pw2_label = 202
        nhp_pw2_label_for_list = (nhp_pw2_label<<12) | 64
        
        label_list = [nhp_pw2_label_for_list]
        
        tunnel_id_pw2 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=True, encap_cw_en=True, encap_ttl_val=0, encap_tagged_vlan=200, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe2 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw2, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        
        pop_nums = 1
        inseg_nhop_pw2 = pw2_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw2_label, pop_nums, None, inseg_nhop_pw2, packet_action, tunnel_id=tunnel_id_pw2)
        pw2_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw2, bridge_id=bridge_id)
        
        pw3_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw3_label = 103
        nhp_pw3_label = 202
        nhp_pw3_label_for_list = (nhp_pw3_label<<12) | 64
        label_list = [nhp_pw3_label_for_list]

        tunnel_id_pw3 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=True, encap_cw_en=True, encap_ttl_val=0, encap_tagged_vlan=300, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe3 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw3, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw3 = pw3_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw3_label, pop_nums, None, inseg_nhop_pw3, packet_action, tunnel_id=tunnel_id_pw3)
        pw3_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw3, bridge_id=bridge_id)

        pw4_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw4_label = 104
        nhp_pw4_label = 202
        nhp_pw4_label_for_list = (nhp_pw4_label<<12) | 64
        label_list = [nhp_pw4_label_for_list]

        tunnel_id_pw4 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=True, encap_cw_en=True, encap_ttl_val=0, encap_tagged_vlan=400, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe4 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw4, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw4 = pw4_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw4_label, pop_nums, None, inseg_nhop_pw4, packet_action, tunnel_id=tunnel_id_pw4)
        pw4_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw4, bridge_id=bridge_id)

        bport_pw_oid_list=[pw2_tunnel_bport_oid, pw3_tunnel_bport_oid, pw4_tunnel_bport_oid]
        nhp_pw_label_list=[nhp_pw2_label, nhp_pw3_label, nhp_pw4_label]
        inseg_pw_label_list=[inseg_pw2_label,inseg_pw3_label,inseg_pw4_label]
        sys_logging("4.Set Provider mpls uni port configuration")
        uni_vlan_id_list = [1001,1002]
        uni_port_oid_list=[0 for i in range(2)]
        tagged_vlan_list=[200,300,400]
        
        for num in range(2):
            uni_port=port_list[num+1]
            uni_vlan_id = uni_vlan_id_list[num]
            uni_port_oid = sai_thrift_create_bridge_sub_port(self.client, uni_port, bridge_id, uni_vlan_id)
            uni_port_oid_list[num] = uni_port_oid
        sys_logging("5.Set FDB entry")
        mac_host_remote_list = ['00:88:88:99:01:01','00:88:88:99:02:01','00:88:88:99:03:01']
        mac_host_local_list = ['00:88:88:88:01:01','00:88:88:88:02:01']
        ip_host_remote_list = ['1.1.1.102','1.1.1.103','1.1.1.104']
        ip_host_local_list = ['1.1.1.1','1.1.1.2']
        mac_action = SAI_PACKET_ACTION_FORWARD
        
        for num in range(3):
            mac_host_remote = mac_host_remote_list[num]
            bport_pw_oid = bport_pw_oid_list[num]
            sai_thrift_create_fdb_bport(self.client, bridge_id, mac_host_remote, bport_pw_oid, mac_action)
        for num in range(2):
            uni_port_oid = uni_port_oid_list[num]
            mac_host_local = mac_host_local_list[num]
            sai_thrift_create_fdb_bport(self.client, bridge_id, mac_host_local, uni_port_oid, mac_action)
        warmboot(self.client)
        sys_logging("9.Send encap and decap traffic packet,check packet receiving")
        dmac_P = provider_nexthop_mac_list[0]
        sys_logging("9.1 Encap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_remote_list[num],
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_remote_list[num],
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            nhp_pw_label = nhp_pw_label_list[num]
            mpls_label_stack = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw_label,'tc':0,'ttl':64,'s':1},{'label':0,'tc':0,'ttl':0,'s':0}]
            pw_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(1, str(uni_pkt))
            self.ctc_verify_packet(pw_pkt, 0)
        sys_logging("9.2 Decap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            inseg_pw_label = inseg_pw_label_list[num]
            mpls_label_stack = [{'label':inseg_lsp_label,'tc':0,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':64,'s':1},{'label':0,'tc':0,'ttl':0,'s':0}]
            pw_pkt = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(0, str(pw_pkt))
            self.ctc_verify_packet(uni_pkt, 1)    
        sys_logging("10.Clear configuration")
        for num in range(2):
            uni_port_oid = uni_port_oid_list[num]
            mac_host_local = mac_host_local_list[num]
            sai_thrift_delete_fdb(self.client, bridge_id, mac_host_local, uni_port_oid)
        for num in range(3):
            mac_host_remote = mac_host_remote_list[num]
            bport_pw_oid = bport_pw_oid_list[num]
            sai_thrift_delete_fdb(self.client, bridge_id, mac_host_remote, bport_pw_oid)
        for num in range(2):
            uni_port=port_list[num+1]
            uni_port_oid = uni_port_oid_list[num]
            sai_thrift_remove_bridge_sub_port(self.client, uni_port_oid, uni_port)
        self.client.sai_thrift_remove_bridge_port(pw4_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw4_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe4)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw4)
        self.client.sai_thrift_remove_router_interface(pw4_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw3_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe3)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw3)
        self.client.sai_thrift_remove_router_interface(pw3_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw2_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe2)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw2)
        self.client.sai_thrift_remove_router_interface(pw2_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge(bridge_id)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_lsp_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_lsp_pe1_to_p)
        self.client.sai_thrift_remove_router_interface(mpls_rif_oid)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            provider_nhop_oid = provider_nhop_oid_list[num]
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            self.client.sai_thrift_remove_next_hop(provider_nhop_oid)
            sai_thrift_delete_fdb(self.client, vlan_oid, nexthop_mac, tmp_port)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
            sai_thrift_remove_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            self.client.sai_thrift_remove_router_interface(provider_rif_oid)
        for num in range(1):
            vlan_oid = provider_vlan_oid_list[num]
            vlan_id = provider_vlan_list[num]
            sai_thrift_vlan_remove_all_ports(self.client, vlan_oid)
            self.client.sai_thrift_remove_vlan(vlan_oid)
            
class Scenario006_MplsVpnTestVplsEvpnEsUc(sai_base_test.ThriftInterfaceDataPlane):
    def runTest(self):
        """
        1.Set provider routing configuration
        2.Set provider mpls lsp configuration
        3.Set provider mpls pw configuration
        4.Set Provider mpls uni port configuration
        5.Set FDB entry
        9.Send encap and decap traffic packet,check packet receiving
        10.Clear configuration
        11.Repeat 1-10 for 3 times
        """
        sys_logging("1.Set provider routing configuration")
        switch_init(self.client)
        provider_vlan_list = [100]
        provider_vlan_oid_list = [0 for i in range(1)]
        provider_vlan_member_oid_list = [0 for i in range(1)]
        for num in range(1):
            vlan_id = provider_vlan_list[num]
            vlan_oid = sai_thrift_create_vlan(self.client, vlan_id)
            provider_vlan_oid_list[num] = vlan_oid
            tmp_port = port_list[num]
            vlan_member = sai_thrift_create_vlan_member(self.client, vlan_oid, tmp_port, SAI_VLAN_TAGGING_MODE_TAGGED)
            provider_vlan_member_oid_list[num] = vlan_member
        v4_enabled = 1
        v6_enabled = 1
        default_vrf_oid = 0x0000000003
        mac=router_mac
        provider_rif_oid_list = [0 for i in range(1)]
        for num in range(1):
            tmp_vlan_oid = provider_vlan_oid_list[num]
            provider_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_VLAN, 0, tmp_vlan_oid, v4_enabled, v6_enabled, mac)
            provider_rif_oid_list[num] = provider_rif_oid
        addr_family = SAI_IP_ADDR_FAMILY_IPV4
        ip_mask_32 = '255.255.255.255'
        ip_mask_24 = '255.255.255.0'
        provider_rif_ip_addr_list = ['192.168.1.1']
        provider_nexthop_ip_addr_list = ['192.168.1.2']
        provider_nexthop_mac_list = ['00:11:22:33:44:01']
        provider_nhop_oid_list = [0 for i in range(1)]
        mac_action = SAI_PACKET_ACTION_FORWARD
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0, packet_action=SAI_PACKET_ACTION_TRAP)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            ret = sai_thrift_create_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_neighbor failed!!! ret = %d" % ret)
            provider_nhop_oid = sai_thrift_create_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid)
            provider_nhop_oid_list[num] = provider_nhop_oid
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            sai_thrift_create_fdb(self.client, vlan_oid, nexthop_mac, tmp_port, mac_action)
        sys_logging("2.Set provider mpls lsp configuration")
        mpls_if_mac = ''
        mpls_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        nhp_lsp_label = 200
        nhp_lsp_label_for_nhp = (nhp_lsp_label<<12) | 64
        nhp_lsp_label_list = [nhp_lsp_label_for_nhp]
        nexthop_ip_addr = provider_nexthop_ip_addr_list[0]
        provider_rif_oid = provider_rif_oid_list[0]
        
        nhop_lsp_pe1_to_p = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, nhp_lsp_label_list)
        
        inseg_lsp_label = 100
        pop_nums = 1
        inseg_nhop_lsp = mpls_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_lsp_label, pop_nums, None, inseg_nhop_lsp, packet_action)
        
        sys_logging("3.Set provider mpls pw configuration")
        bridge_id = sai_thrift_create_bridge(self.client, SAI_BRIDGE_TYPE_1D)
        sys_logging(">>bridge_id = %d" % bridge_id)
        
        pw2_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw2_label = 102
        nhp_pw2_label = 202
        nhp_pw2_label_for_list = (nhp_pw2_label<<12) | 64
        
        label_list = [nhp_pw2_label_for_list]
        
        tunnel_id_pw2 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=True, encap_cw_en=True, encap_ttl_val=0, encap_tagged_vlan=200, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe2 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw2, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        
        pop_nums = 1
        inseg_nhop_pw2 = pw2_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw2_label, pop_nums, None, inseg_nhop_pw2, packet_action, tunnel_id=tunnel_id_pw2)
        pw2_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw2, bridge_id=bridge_id)
        
        pw3_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw3_label = 103
        nhp_pw3_label = 202
        nhp_pw3_label_for_list = (nhp_pw3_label<<12) | 64
        label_list = [nhp_pw3_label_for_list]

        tunnel_id_pw3 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=True, encap_cw_en=True, encap_ttl_val=0, encap_tagged_vlan=300, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe3 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw3, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw3 = pw3_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw3_label, pop_nums, None, inseg_nhop_pw3, packet_action, tunnel_id=tunnel_id_pw3)
        pw3_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw3, bridge_id=bridge_id)

        pw4_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw4_label = 104
        nhp_pw4_label = 202
        nhp_pw4_label_for_list = (nhp_pw4_label<<12) | 64
        label_list = [nhp_pw4_label_for_list]

        tunnel_id_pw4 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=True, encap_cw_en=True, encap_ttl_val=0, encap_tagged_vlan=400, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe4 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw4, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw4 = pw4_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw4_label, pop_nums, None, inseg_nhop_pw4, packet_action, tunnel_id=tunnel_id_pw4)
        pw4_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw4, bridge_id=bridge_id)

        bport_pw_oid_list=[pw2_tunnel_bport_oid, pw3_tunnel_bport_oid, pw4_tunnel_bport_oid]
        nhp_pw_label_list=[nhp_pw2_label, nhp_pw3_label, nhp_pw4_label]
        inseg_pw_label_list=[inseg_pw2_label,inseg_pw3_label,inseg_pw4_label]
        sys_logging("4.Set Provider mpls uni port configuration")
        uni_vlan_id_list = [1001,1002]
        uni_port_oid_list=[0 for i in range(2)]
        tagged_vlan_list=[200,300,400]
        
        for num in range(2):
            uni_port=port_list[num+1]
            uni_vlan_id = uni_vlan_id_list[num]
            uni_port_oid = sai_thrift_create_bridge_sub_port(self.client, uni_port, bridge_id, uni_vlan_id)
            uni_port_oid_list[num] = uni_port_oid
        sys_logging("5.Set FDB entry")
        mac_host_remote_list = ['00:88:88:99:01:01','00:88:88:99:02:01','00:88:88:99:03:01']
        mac_host_local_list = ['00:88:88:88:01:01','00:88:88:88:02:01']
        ip_host_remote_list = ['1.1.1.102','1.1.1.103','1.1.1.104']
        ip_host_local_list = ['1.1.1.1','1.1.1.2']
        mac_action = SAI_PACKET_ACTION_FORWARD
        
        for num in range(3):
            mac_host_remote = mac_host_remote_list[num]
            bport_pw_oid = bport_pw_oid_list[num]
            sai_thrift_create_fdb_bport(self.client, bridge_id, mac_host_remote, bport_pw_oid, mac_action)
        for num in range(2):
            uni_port_oid = uni_port_oid_list[num]
            mac_host_local = mac_host_local_list[num]
            sai_thrift_create_fdb_bport(self.client, bridge_id, mac_host_local, uni_port_oid, mac_action)
            
        sys_logging("7.Create ES")
        esi_label=10000
        es_oid = sai_thrift_create_es(self.client, esi_label)
        esi_label2=10001
        es2_oid = sai_thrift_create_es(self.client, esi_label2)
        
        attrs = self.client.sai_thrift_get_es_attribute(es_oid)
        for a in attrs.attr_list:
            if a.id == SAI_PORT_ATTR_ES:
                u32 = ctypes.c_uint32(attribute.value.u32)
                sys_logging("### SAI_ES_ATTR_ESI_LABEL = %d ###"  %u32.value)
                assert (esi_label == u32.value)
        attrs = self.client.sai_thrift_get_es_attribute(es2_oid)
        for a in attrs.attr_list:
            if a.id == SAI_PORT_ATTR_ES:
                u32 = ctypes.c_uint32(attribute.value.u32)
                sys_logging("### SAI_ES_ATTR_ESI_LABEL = %d ###"  %u32.value)
                assert (esi_label2 == u32.value)
        sys_logging("8.Add port into ES")
        port_oid = port_list[1]
        attrs = self.client.sai_thrift_get_port_attribute(port_oid)
        for a in attrs.attr_list:
            if a.id == SAI_PORT_ATTR_ES:
                sys_logging("### SAI_PORT_ATTR_ES = %x ###"  %a.value.oid)
                assert (SAI_NULL_OBJECT_ID == a.value.oid)    
        attr_value = sai_thrift_attribute_value_t(oid=es_oid)
        attr = sai_thrift_attribute_t(id=SAI_PORT_ATTR_ES, value=attr_value)
        self.client.sai_thrift_set_port_attribute(port_oid, attr)
        
        attrs = self.client.sai_thrift_get_port_attribute(port_oid)
        for a in attrs.attr_list:
            if a.id == SAI_PORT_ATTR_ES:
                sys_logging("### SAI_PORT_ATTR_ES = %x ###"  %a.value.oid)
                assert (es_oid == a.value.oid)
        warmboot(self.client)
        sys_logging("9.Send encap and decap traffic packet,check packet receiving")
        dmac_P = provider_nexthop_mac_list[0]
        sys_logging("9.1 Encap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_remote_list[num],
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_remote_list[num],
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            nhp_pw_label = nhp_pw_label_list[num]
            mpls_label_stack = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw_label,'tc':0,'ttl':64,'s':1},{'label':0,'tc':0,'ttl':0,'s':0}]
            pw_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(1, str(uni_pkt))
            self.ctc_verify_packet(pw_pkt, 0)
        sys_logging("9.2 Decap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            inseg_pw_label = inseg_pw_label_list[num]
            mpls_label_stack = [{'label':inseg_lsp_label,'tc':0,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':64,'s':1},{'label':0,'tc':0,'ttl':0,'s':0}]
            pw_pkt = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(0, str(pw_pkt))
            self.ctc_verify_packet(uni_pkt, 1)    
        sys_logging("10.Clear configuration")
        attr_value = sai_thrift_attribute_value_t(oid=SAI_NULL_OBJECT_ID)
        attr = sai_thrift_attribute_t(id=SAI_PORT_ATTR_ES, value=attr_value)
        self.client.sai_thrift_set_port_attribute(port_oid, attr)
        self.client.sai_thrift_remove_es(es2_oid)
        self.client.sai_thrift_remove_es(es_oid)
        for num in range(2):
            uni_port_oid = uni_port_oid_list[num]
            mac_host_local = mac_host_local_list[num]
            sai_thrift_delete_fdb(self.client, bridge_id, mac_host_local, uni_port_oid)
        for num in range(3):
            mac_host_remote = mac_host_remote_list[num]
            bport_pw_oid = bport_pw_oid_list[num]
            sai_thrift_delete_fdb(self.client, bridge_id, mac_host_remote, bport_pw_oid)
        for num in range(2):
            uni_port=port_list[num+1]
            uni_port_oid = uni_port_oid_list[num]
            sai_thrift_remove_bridge_sub_port(self.client, uni_port_oid, uni_port)
        self.client.sai_thrift_remove_bridge_port(pw4_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw4_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe4)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw4)
        self.client.sai_thrift_remove_router_interface(pw4_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw3_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe3)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw3)
        self.client.sai_thrift_remove_router_interface(pw3_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw2_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe2)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw2)
        self.client.sai_thrift_remove_router_interface(pw2_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge(bridge_id)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_lsp_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_lsp_pe1_to_p)
        self.client.sai_thrift_remove_router_interface(mpls_rif_oid)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            provider_nhop_oid = provider_nhop_oid_list[num]
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            self.client.sai_thrift_remove_next_hop(provider_nhop_oid)
            sai_thrift_delete_fdb(self.client, vlan_oid, nexthop_mac, tmp_port)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
            sai_thrift_remove_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            self.client.sai_thrift_remove_router_interface(provider_rif_oid)
        for num in range(1):
            vlan_oid = provider_vlan_oid_list[num]
            vlan_id = provider_vlan_list[num]
            sai_thrift_vlan_remove_all_ports(self.client, vlan_oid)
            self.client.sai_thrift_remove_vlan(vlan_oid)
            
class Scenario007_MplsVpnTestVplsEvpnEsBum(sai_base_test.ThriftInterfaceDataPlane):
    def runTest(self):
        """
        1.Set provider routing configuration
        2.Set provider mpls lsp configuration
        3.Set provider mpls pw configuration
        4.Set Provider mpls uni port configuration
        5.Set FDB entry
        9.Send encap and decap traffic packet,check packet receiving
        10.Clear configuration
        11.Repeat 1-10 for 3 times
        """
        sys_logging("1.Set provider routing configuration")
        switch_init(self.client)
        provider_vlan_list = [100]
        provider_vlan_oid_list = [0 for i in range(1)]
        provider_vlan_member_oid_list = [0 for i in range(1)]
        for num in range(1):
            vlan_id = provider_vlan_list[num]
            vlan_oid = sai_thrift_create_vlan(self.client, vlan_id)
            provider_vlan_oid_list[num] = vlan_oid
            tmp_port = port_list[num]
            vlan_member = sai_thrift_create_vlan_member(self.client, vlan_oid, tmp_port, SAI_VLAN_TAGGING_MODE_TAGGED)
            provider_vlan_member_oid_list[num] = vlan_member
        v4_enabled = 1
        v6_enabled = 1
        default_vrf_oid = 0x0000000003
        mac=router_mac
        provider_rif_oid_list = [0 for i in range(1)]
        for num in range(1):
            tmp_vlan_oid = provider_vlan_oid_list[num]
            provider_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_VLAN, 0, tmp_vlan_oid, v4_enabled, v6_enabled, mac)
            provider_rif_oid_list[num] = provider_rif_oid
        addr_family = SAI_IP_ADDR_FAMILY_IPV4
        ip_mask_32 = '255.255.255.255'
        ip_mask_24 = '255.255.255.0'
        provider_rif_ip_addr_list = ['192.168.1.1']
        provider_nexthop_ip_addr_list = ['192.168.1.2']
        provider_nexthop_mac_list = ['00:11:22:33:44:01']
        provider_nhop_oid_list = [0 for i in range(1)]
        mac_action = SAI_PACKET_ACTION_FORWARD
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0, packet_action=SAI_PACKET_ACTION_TRAP)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            ret = sai_thrift_create_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_neighbor failed!!! ret = %d" % ret)
            provider_nhop_oid = sai_thrift_create_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid)
            provider_nhop_oid_list[num] = provider_nhop_oid
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            sai_thrift_create_fdb(self.client, vlan_oid, nexthop_mac, tmp_port, mac_action)
        sys_logging("2.Set provider mpls lsp configuration")
        mpls_if_mac = ''
        mpls_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        nhp_lsp_label = 200
        nhp_lsp_label_for_nhp = (nhp_lsp_label<<12) | 64
        nhp_lsp_label_list = [nhp_lsp_label_for_nhp]
        nexthop_ip_addr = provider_nexthop_ip_addr_list[0]
        provider_rif_oid = provider_rif_oid_list[0]
        
        nhop_lsp_pe1_to_p = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, nhp_lsp_label_list)
        
        inseg_lsp_label = 100
        pop_nums = 1
        inseg_nhop_lsp = mpls_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_lsp_label, pop_nums, None, inseg_nhop_lsp, packet_action)
        
        sys_logging("3.Set provider mpls pw configuration")
        bridge_id = sai_thrift_create_bridge(self.client, SAI_BRIDGE_TYPE_1D)
        sys_logging(">>bridge_id = %d" % bridge_id)
        
        pw2_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw2_label = 102
        nhp_pw2_label = 202
        nhp_pw2_label_for_list = (nhp_pw2_label<<12) | 64
        
        label_list = [nhp_pw2_label_for_list]
        
        tunnel_id_pw2 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=False, encap_cw_en=False, encap_ttl_val=0, encap_tagged_vlan=200, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe2 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw2, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        
        pop_nums = 1
        inseg_nhop_pw2 = pw2_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw2_label, pop_nums, None, inseg_nhop_pw2, packet_action, tunnel_id=tunnel_id_pw2)
        pw2_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw2, bridge_id=bridge_id)
        
        pw3_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw3_label = 103
        nhp_pw3_label = 202
        nhp_pw3_label_for_list = (nhp_pw3_label<<12) | 64
        label_list = [nhp_pw3_label_for_list]

        tunnel_id_pw3 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=False, encap_cw_en=False, encap_ttl_val=0, encap_tagged_vlan=300, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe3 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw3, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw3 = pw3_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw3_label, pop_nums, None, inseg_nhop_pw3, packet_action, tunnel_id=tunnel_id_pw3)
        pw3_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw3, bridge_id=bridge_id)

        pw4_bridge_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac, dot1d_bridge_id=bridge_id)
        inseg_pw4_label = 104
        nhp_pw4_label = 202
        nhp_pw4_label_for_list = (nhp_pw4_label<<12) | 64
        label_list = [nhp_pw4_label_for_list]

        tunnel_id_pw4 = sai_thrift_create_tunnel_mpls_l2vpn(self.client, decap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, encap_pw_mode=SAI_TUNNEL_MPLS_PW_MODE_TAGGED, decap_cw_en=False, encap_cw_en=False, encap_ttl_val=0, encap_tagged_vlan=400, decap_esi_label_valid=True, encap_esi_label_valid=True)
        nhop_pw_pe1_to_pe4 = sai_thrift_create_tunnel_mpls_l2vpn_nhop(self.client, tunnel_id_pw4, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p)
        pop_nums = 1
        inseg_nhop_pw4 = pw4_bridge_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw4_label, pop_nums, None, inseg_nhop_pw4, packet_action, tunnel_id=tunnel_id_pw4)
        pw4_tunnel_bport_oid = sai_thrift_create_bridge_tunnel_port(self.client, tunnel_id=tunnel_id_pw4, bridge_id=bridge_id)

        bport_pw_oid_list=[pw2_tunnel_bport_oid, pw3_tunnel_bport_oid, pw4_tunnel_bport_oid]
        nhp_pw_label_list=[nhp_pw2_label, nhp_pw3_label, nhp_pw4_label]
        inseg_pw_label_list=[inseg_pw2_label,inseg_pw3_label,inseg_pw4_label]
        sys_logging("4.Set Provider mpls uni port configuration")
        uni_vlan_id_list = [1001,1002]
        uni_port_oid_list=[0 for i in range(2)]
        tagged_vlan_list=[200,300,400]
        
        for num in range(2):
            uni_port=port_list[num+1]
            uni_vlan_id = uni_vlan_id_list[num]
            uni_port_oid = sai_thrift_create_bridge_sub_port(self.client, uni_port, bridge_id, uni_vlan_id)
            uni_port_oid_list[num] = uni_port_oid
        sys_logging("5.Set FDB entry")
        unknown_remote_mac = '00:88:88:99:01:ff'
        mac_host_remote_list = ['00:88:88:99:01:01','00:88:88:99:02:01','00:88:88:99:03:01']
        mac_host_local_list = ['00:88:88:88:01:01','00:88:88:88:02:01']
        ip_host_remote_list = ['1.1.1.102','1.1.1.103','1.1.1.104']
        ip_host_local_list = ['1.1.1.1','1.1.1.2']
        mac_action = SAI_PACKET_ACTION_FORWARD
        
        for num in range(3):
            mac_host_remote = mac_host_remote_list[num]
            bport_pw_oid = bport_pw_oid_list[num]
            sai_thrift_create_fdb_bport(self.client, bridge_id, mac_host_remote, bport_pw_oid, mac_action)
        for num in range(2):
            uni_port_oid = uni_port_oid_list[num]
            mac_host_local = mac_host_local_list[num]
            sai_thrift_create_fdb_bport(self.client, bridge_id, mac_host_local, uni_port_oid, mac_action)

        sys_logging("7.Create ES")
        esi_label=10000
        es_oid = sai_thrift_create_es(self.client, esi_label)
        esi_label2=10001
        es2_oid = sai_thrift_create_es(self.client, esi_label2)
        
        attrs = self.client.sai_thrift_get_es_attribute(es_oid)
        for a in attrs.attr_list:
            if a.id == SAI_PORT_ATTR_ES:
                u32 = ctypes.c_uint32(attribute.value.u32)
                sys_logging("### SAI_ES_ATTR_ESI_LABEL = %d ###"  %u32.value)
                assert (esi_label == u32.value)
        attrs = self.client.sai_thrift_get_es_attribute(es2_oid)
        for a in attrs.attr_list:
            if a.id == SAI_PORT_ATTR_ES:
                u32 = ctypes.c_uint32(attribute.value.u32)
                sys_logging("### SAI_ES_ATTR_ESI_LABEL = %d ###"  %u32.value)
                assert (esi_label2 == u32.value)
        sys_logging("8.Add port into ES")
        port_oid = port_list[1]
        attrs = self.client.sai_thrift_get_port_attribute(port_oid)
        for a in attrs.attr_list:
            if a.id == SAI_PORT_ATTR_ES:
                sys_logging("### SAI_PORT_ATTR_ES = %x ###"  %a.value.oid)
                assert (SAI_NULL_OBJECT_ID == a.value.oid)    
        attr_value = sai_thrift_attribute_value_t(oid=es_oid)
        attr = sai_thrift_attribute_t(id=SAI_PORT_ATTR_ES, value=attr_value)
        self.client.sai_thrift_set_port_attribute(port_oid, attr)
        
        attrs = self.client.sai_thrift_get_port_attribute(port_oid)
        for a in attrs.attr_list:
            if a.id == SAI_PORT_ATTR_ES:
                sys_logging("### SAI_PORT_ATTR_ES = %x ###"  %a.value.oid)
                assert (es_oid == a.value.oid)
        warmboot(self.client)
        sys_logging("9.Send encap and decap traffic packet,check packet receiving")
        dmac_P = provider_nexthop_mac_list[0]
        sys_logging("9.1 Encap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=unknown_remote_mac,
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            uni2_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=unknown_remote_mac,
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=1002,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt1 = simple_tcp_packet(pktlen=96,
                                eth_dst=unknown_remote_mac,
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt2 = simple_tcp_packet(pktlen=96,
                                eth_dst=unknown_remote_mac,
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=300,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt3 = simple_tcp_packet(pktlen=96,
                                eth_dst=unknown_remote_mac,
                                eth_src=mac_host_local_list[0],
                                dl_vlan_enable=True,
                                vlan_vid=400,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_remote_list[num],
                                ip_src=ip_host_local_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            nhp_pw2_label = nhp_pw_label_list[0]
            nhp_pw3_label = nhp_pw_label_list[1]
            nhp_pw4_label = nhp_pw_label_list[2]
            mpls_label_stack1 = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw2_label,'tc':0,'ttl':64,'s':0},{'label':10000,'tc':0,'ttl':64,'s':1}]
            mpls_label_stack2 = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw3_label,'tc':0,'ttl':64,'s':0},{'label':10000,'tc':0,'ttl':64,'s':1}]
            mpls_label_stack3 = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw4_label,'tc':0,'ttl':64,'s':0},{'label':10000,'tc':0,'ttl':64,'s':1}]
            pw2_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack1,
                                inner_frame = mpls_inner_pkt1)
            pw3_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack2,
                                inner_frame = mpls_inner_pkt2)
            pw4_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack3,
                                inner_frame = mpls_inner_pkt3)
            self.ctc_send_packet(1, str(uni_pkt))
            self.ctc_verify_packet(pw2_pkt, 0,1)
            self.ctc_verify_packet(pw3_pkt, 0,2)
            self.ctc_verify_packet(pw4_pkt, 0,3)
            self.ctc_verify_packet(uni2_pkt, 2)
        sys_logging("9.2 Decap")
        for num in range(1):
            uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=1001,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            mpls_inner_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_host_local_list[0],
                                eth_src=mac_host_remote_list[num],
                                dl_vlan_enable=True,
                                vlan_vid=200,
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=ip_host_local_list[num],
                                ip_src=ip_host_remote_list[num],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5)
            inseg_pw_label = inseg_pw_label_list[num]
            mpls_label_stack1 = [{'label':inseg_lsp_label,'tc':0,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':64,'s':0},{'label':10000,'tc':0,'ttl':64,'s':1}]
            mpls_label_stack2 = [{'label':inseg_lsp_label,'tc':0,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':64,'s':0},{'label':10001,'tc':0,'ttl':64,'s':1}]
            mpls_label_stack3 = [{'label':inseg_lsp_label,'tc':0,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':0,'ttl':64,'s':0},{'label':10002,'tc':0,'ttl':64,'s':1}]
            pw_pkt1 = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack1,
                                inner_frame = mpls_inner_pkt)
            pw_pkt2 = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack2,
                                inner_frame = mpls_inner_pkt)
            pw_pkt3 = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack3,
                                inner_frame = mpls_inner_pkt)
            self.ctc_send_packet(0, str(pw_pkt1))
            self.ctc_verify_no_packet(uni_pkt, 1)
            self.ctc_send_packet(0, str(pw_pkt3))
            self.ctc_verify_no_packet(uni_pkt, 1)  
            self.ctc_send_packet(0, str(pw_pkt2))
            self.ctc_verify_packet(uni_pkt, 1)  
            
        sys_logging("10.Clear configuration")
        attr_value = sai_thrift_attribute_value_t(oid=SAI_NULL_OBJECT_ID)
        attr = sai_thrift_attribute_t(id=SAI_PORT_ATTR_ES, value=attr_value)
        self.client.sai_thrift_set_port_attribute(port_oid, attr)
        self.client.sai_thrift_remove_es(es2_oid)
        self.client.sai_thrift_remove_es(es_oid)
        for num in range(2):
            uni_port_oid = uni_port_oid_list[num]
            mac_host_local = mac_host_local_list[num]
            sai_thrift_delete_fdb(self.client, bridge_id, mac_host_local, uni_port_oid)
        for num in range(3):
            mac_host_remote = mac_host_remote_list[num]
            bport_pw_oid = bport_pw_oid_list[num]
            sai_thrift_delete_fdb(self.client, bridge_id, mac_host_remote, bport_pw_oid)
        for num in range(2):
            uni_port=port_list[num+1]
            uni_port_oid = uni_port_oid_list[num]
            sai_thrift_remove_bridge_sub_port(self.client, uni_port_oid, uni_port)
        self.client.sai_thrift_remove_bridge_port(pw4_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw4_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe4)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw4)
        self.client.sai_thrift_remove_router_interface(pw4_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw3_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe3)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw3)
        self.client.sai_thrift_remove_router_interface(pw3_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge_port(pw2_tunnel_bport_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe2)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw2)
        self.client.sai_thrift_remove_router_interface(pw2_bridge_rif_oid)
        self.client.sai_thrift_remove_bridge(bridge_id)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_lsp_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_lsp_pe1_to_p)
        self.client.sai_thrift_remove_router_interface(mpls_rif_oid)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            provider_nhop_oid = provider_nhop_oid_list[num]
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            self.client.sai_thrift_remove_next_hop(provider_nhop_oid)
            sai_thrift_delete_fdb(self.client, vlan_oid, nexthop_mac, tmp_port)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
            sai_thrift_remove_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            self.client.sai_thrift_remove_router_interface(provider_rif_oid)
        for num in range(1):
            vlan_oid = provider_vlan_oid_list[num]
            vlan_id = provider_vlan_list[num]
            sai_thrift_vlan_remove_all_ports(self.client, vlan_oid)
            self.client.sai_thrift_remove_vlan(vlan_oid)
            
class Scenario008_MplsVpnTestL3vpnPipe(sai_base_test.ThriftInterfaceDataPlane):
    def runTest(self):
        """
        1.Set provider routing configuration
        2.Set provider mpls lsp configuration
        3.Set provider mpls pw configuration
        4.Set Provider mpls uni interface configuration
        5.Set FIB entry
        9.Send encap and decap traffic packet,check packet receiving
        10.Clear configuration
        11.Repeat 1-10 for 3 times
        """
        sys_logging("1.Set provider routing configuration")
        switch_init(self.client)
        provider_vlan_list = [100]
        provider_vlan_oid_list = [0 for i in range(1)]
        provider_vlan_member_oid_list = [0 for i in range(1)]
        for num in range(1):
            vlan_id = provider_vlan_list[num]
            vlan_oid = sai_thrift_create_vlan(self.client, vlan_id)
            provider_vlan_oid_list[num] = vlan_oid
            tmp_port = port_list[num]
            vlan_member = sai_thrift_create_vlan_member(self.client, vlan_oid, tmp_port, SAI_VLAN_TAGGING_MODE_TAGGED)
            provider_vlan_member_oid_list[num] = vlan_member
        v4_enabled = 1
        v6_enabled = 1
        default_vrf_oid = 0x0000000003
        mac=router_mac
        provider_rif_oid_list = [0 for i in range(1)]
        for num in range(1):
            tmp_vlan_oid = provider_vlan_oid_list[num]
            provider_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_VLAN, 0, tmp_vlan_oid, v4_enabled, v6_enabled, mac)
            provider_rif_oid_list[num] = provider_rif_oid
        addr_family = SAI_IP_ADDR_FAMILY_IPV4
        ip_mask_32 = '255.255.255.255'
        ip_mask_24 = '255.255.255.0'
        provider_rif_ip_addr_list = ['192.168.1.1']
        provider_nexthop_ip_addr_list = ['192.168.1.2']
        provider_nexthop_mac_list = ['00:11:22:33:44:01']
        provider_nhop_oid_list = [0 for i in range(1)]
        mac_action = SAI_PACKET_ACTION_FORWARD
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0, packet_action=SAI_PACKET_ACTION_TRAP)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            ret = sai_thrift_create_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_neighbor failed!!! ret = %d" % ret)
            provider_nhop_oid = sai_thrift_create_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid)
            provider_nhop_oid_list[num] = provider_nhop_oid
            ret = sai_thrift_create_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_create_route failed!!! ret = %d" % ret)
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            sai_thrift_create_fdb(self.client, vlan_oid, nexthop_mac, tmp_port, mac_action)
        sys_logging("2.Set provider mpls lsp configuration")
        mpls_if_mac = ''
        mpls_rif_oid = sai_thrift_create_router_interface(self.client, default_vrf_oid, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        nhp_lsp_label = 200
        nhp_lsp_label_for_nhp = (nhp_lsp_label<<12) | 64
        nhp_lsp_label_list = [nhp_lsp_label_for_nhp]
        nexthop_ip_addr = provider_nexthop_ip_addr_list[0]
        provider_rif_oid = provider_rif_oid_list[0]
        nhop_lsp_pe1_to_p = sai_thrift_create_mpls_nhop(self.client, addr_family, nexthop_ip_addr, provider_rif_oid, nhp_lsp_label_list)
        
        inseg_lsp_label = 100
        pop_nums = 1
        inseg_nhop_lsp = mpls_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        
        sai_thrift_create_inseg_entry(self.client, inseg_lsp_label, pop_nums, None, inseg_nhop_lsp, packet_action)
        
        sys_logging("3.Set provider mpls pw configuration")
        pw2_vr_id = sai_thrift_create_virtual_router(self.client, v4_enabled, v6_enabled)
        pw3_vr_id = sai_thrift_create_virtual_router(self.client, v4_enabled, v6_enabled)
        pw2_rif_oid = sai_thrift_create_router_interface(self.client, pw2_vr_id, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        pw3_rif_oid = sai_thrift_create_router_interface(self.client, pw3_vr_id, SAI_ROUTER_INTERFACE_TYPE_MPLS_ROUTER, 0, 0, v4_enabled, v6_enabled, mpls_if_mac)
        
        inseg_pw2_label = 102
        nhp_pw2_label = 202
        nhp_pw2_label_for_list = (nhp_pw2_label<<12) | 64
        label_list = [nhp_pw2_label_for_list]
        tunnel_id_pw2 = sai_thrift_create_tunnel_mpls(self.client,decap_ttl_mode=SAI_TUNNEL_TTL_MODE_PIPE_MODEL, encap_ttl_mode=SAI_TUNNEL_TTL_MODE_PIPE_MODEL, encap_ttl_val=128, decap_exp_mode=SAI_TUNNEL_EXP_MODE_PIPE_MODEL, encap_exp_mode=SAI_TUNNEL_EXP_MODE_PIPE_MODEL, encap_exp_val=3)

        nhop_pw_pe1_to_pe2 = sai_thrift_create_mpls_nhop(self.client, addr_family, '', None, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p, tunnel_id=tunnel_id_pw2)
        pop_nums = 1
        inseg_nhop_pw2 = pw2_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw2_label, pop_nums, None, inseg_nhop_pw2, packet_action, tunnel_id=tunnel_id_pw2)
        
        inseg_pw3_label = 103
        nhp_pw3_label = 203
        nhp_pw3_label_for_list = (nhp_pw3_label<<12) | 64
        label_list = [nhp_pw3_label_for_list]
        tunnel_id_pw3 = sai_thrift_create_tunnel_mpls(self.client,decap_ttl_mode=SAI_TUNNEL_TTL_MODE_PIPE_MODEL, encap_ttl_mode=SAI_TUNNEL_TTL_MODE_PIPE_MODEL, encap_ttl_val=32, decap_exp_mode=SAI_TUNNEL_EXP_MODE_PIPE_MODEL, encap_exp_mode=SAI_TUNNEL_EXP_MODE_PIPE_MODEL, encap_exp_val=7)

        nhop_pw_pe1_to_pe3 = sai_thrift_create_mpls_nhop(self.client, addr_family, '', None, label_list, next_level_nhop_oid=nhop_lsp_pe1_to_p, tunnel_id=tunnel_id_pw3)
        pop_nums = 1
        inseg_nhop_pw3 = pw3_rif_oid
        packet_action = SAI_PACKET_ACTION_FORWARD
        sai_thrift_create_inseg_entry(self.client, inseg_pw3_label, pop_nums, None, inseg_nhop_pw3, packet_action, tunnel_id=tunnel_id_pw3)

        nhp_pw_label_list=[nhp_pw2_label, nhp_pw3_label]
        inseg_pw_label_list=[inseg_pw2_label,inseg_pw3_label]
        sys_logging("4.Set Provider mpls uni interface configuration")
        uni_vlan_id_list = [1001,1002]
        tagged_vlan_list=[200,300]
        vr_list=[pw2_vr_id,pw3_vr_id]
        rif_uni_oid_list=[[0 for i in range(2)] for i in range(2)]
        for num in range(2):
            uni_port=port_list[num+1]
            for num2 in range(2):
                vr_id = vr_list[num2]
                uni_vlan_id = uni_vlan_id_list[num2]
                rif_uni_oid = sai_thrift_create_router_interface(self.client, vr_id, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT, uni_port, None, v4_enabled, v6_enabled, '', outer_vlan_id=uni_vlan_id)
                rif_uni_oid_list[num][num2] = rif_uni_oid
                
        sys_logging("5.Set FIB entry")
        mac_uni_peer_list = [['00:88:88:99:01:01','00:88:88:99:02:01'],['00:88:88:77:01:01','00:88:88:77:02:01']]
        mac_host_local_list = ['00:88:88:88:01:01','00:88:88:88:02:01']
        pw_side_ip_list = ['1.1.3.2','1.1.1.2']
        uni_side_ip_list = [['1.1.1.2','1.1.2.2'], ['1.1.2.2','1.1.3.2']]
        mac_action = SAI_PACKET_ACTION_FORWARD
        uni_nhop_list = [[0 for i in range(2)] for i in range(2)]
        for num in range(2):
            for num2 in range(2):
                vr_id = vr_list[num2]
                rif_uni_oid = rif_uni_oid_list[num][num2]
                uni_side_ip = uni_side_ip_list[num][num2]
                mac_uni_peer = mac_uni_peer_list[num][num2]
                sai_thrift_create_neighbor(self.client, addr_family, rif_uni_oid, uni_side_ip, mac_uni_peer)
                uni_nhop = sai_thrift_create_nhop(self.client, addr_family, uni_side_ip, rif_uni_oid)
                ret = sai_thrift_create_route(self.client, vr_id, addr_family, uni_side_ip, ip_mask_24, uni_nhop)
                uni_nhop_list[num][num2] = uni_nhop
        nhop_pw_list = [nhop_pw_pe1_to_pe2,nhop_pw_pe1_to_pe3]
        for num in range(2):
            vr_id = vr_list[num]
            pw_side_ip = pw_side_ip_list[num]
            nhop_pw = nhop_pw_list[num]
            ret = sai_thrift_create_route(self.client, vr_id, addr_family, pw_side_ip, ip_mask_24, nhop_pw)
        sys_logging("8.Get and Set attribute")
        ids_list = [SAI_TUNNEL_ATTR_DECAP_TTL_MODE,SAI_TUNNEL_ATTR_ENCAP_TTL_MODE,SAI_TUNNEL_ATTR_ENCAP_TTL_VAL,SAI_TUNNEL_ATTR_DECAP_EXP_MODE,SAI_TUNNEL_ATTR_ENCAP_EXP_MODE,SAI_TUNNEL_ATTR_ENCAP_EXP_VAL]
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw2,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_TTL_MODE_PIPE_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_TTL_MODE_PIPE_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_VAL:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_VAL = %d ###"  % u8.value)
                assert ( 128 == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_EXP_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_EXP_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_EXP_MODE_PIPE_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_EXP_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_EXP_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_EXP_MODE_PIPE_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_EXP_VAL:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_EXP_VAL = %d ###"  % u8.value)
                assert ( 3 == u8.value )
        tunnel_attr_list = self.client.sai_thrift_get_tunnel_attribute(tunnel_id_pw3,ids_list)
        attr_list = tunnel_attr_list.attr_list
        for attribute in attr_list:
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_TTL_MODE_PIPE_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_TTL_MODE_PIPE_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_TTL_VAL:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_TTL_VAL = %d ###"  % u8.value)
                assert ( 32 == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_DECAP_EXP_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_DECAP_EXP_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_EXP_MODE_PIPE_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_EXP_MODE:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_EXP_MODE = %d ###"  % u8.value)
                assert ( SAI_TUNNEL_EXP_MODE_PIPE_MODEL == u8.value )
            if attribute.id == SAI_TUNNEL_ATTR_ENCAP_EXP_VAL:
                u8 = ctypes.c_uint8(attribute.value.u8)
                sys_logging("### SAI_TUNNEL_ATTR_ENCAP_EXP_VAL = %d ###"  % u8.value)
                assert ( 7 == u8.value )
        warmboot(self.client)
        sys_logging("9.Send encap and decap traffic packet,check packet receiving")
        dmac_P = provider_nexthop_mac_list[0]
        sys_logging("9.1 Encap")
        for num in range(1):
            for num2 in range(1):
                uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=router_mac,
                                eth_src=mac_uni_peer_list[num][num2],
                                dl_vlan_enable=True,
                                vlan_vid=uni_vlan_id_list[num2],
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=pw_side_ip_list[num2],
                                ip_src=uni_side_ip_list[num][num2],
                                ip_id=105,
                                ip_ttl=64,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                mpls_inner_pkt = simple_ip_only_packet(pktlen=78,
                                ip_src=uni_side_ip_list[num][num2],
                                ip_dst=pw_side_ip_list[num2],
                                ip_ttl=63,
                                ip_id=105,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                nhp_pw_label = nhp_pw_label_list[num2]
                mpls_label_stack = [{'label':nhp_lsp_label,'tc':0,'ttl':64,'s':0},{'label':nhp_pw_label,'tc':3,'ttl':128,'s':1}]
                pw_pkt = simple_mpls_packet(
                                eth_dst=dmac_P,
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
                self.ctc_send_packet(1, str(uni_pkt))
                self.ctc_verify_packet(pw_pkt, 0)
        sys_logging("9.2 Decap")
        for num in range(1):
            for num2 in range(1):
                uni_pkt = simple_tcp_packet(pktlen=96,
                                eth_dst=mac_uni_peer_list[num][num2],
                                eth_src=router_mac,
                                dl_vlan_enable=True,
                                vlan_vid=uni_vlan_id_list[num2],
                                vlan_pcp=0,
                                dl_vlan_cfi=0,
                                ip_dst=uni_side_ip_list[num][num2],
                                ip_src=pw_side_ip_list[num2],
                                ip_id=105,
                                ip_ttl=39,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                mpls_inner_pkt = simple_ip_only_packet(pktlen=78,
                                ip_src=pw_side_ip_list[num2],
                                ip_dst=uni_side_ip_list[num][num2],
                                ip_ttl=40,
                                ip_id=105,
                                ip_ihl=5,
                                tcp_sport=1234,
                                tcp_dport=80,
                                tcp_flags="S",
                                with_tcp_chksum=True)
                inseg_pw_label = inseg_pw_label_list[num2]
                mpls_label_stack = [{'label':inseg_lsp_label,'tc':6,'ttl':64,'s':0},{'label':inseg_pw_label,'tc':7,'ttl':64,'s':1}]
                pw_pkt = simple_mpls_packet(
                                eth_dst=router_mac,
                                eth_src=dmac_P,
                                dl_vlan_enable=True,
                                vlan_vid=100,
                                vlan_pcp=0,
                                mpls_type=0x8847,
                                mpls_tags= mpls_label_stack,
                                inner_frame = mpls_inner_pkt)
                self.ctc_send_packet(0, str(pw_pkt))
                self.ctc_verify_packet(uni_pkt, 1)    
        sys_logging("10.Clear configuration")
        for num in range(2):
            vr_id = vr_list[num]
            pw_side_ip = pw_side_ip_list[num]
            nhop_pw = nhop_pw_list[num]
            ret = sai_thrift_remove_route(self.client, vr_id, addr_family, pw_side_ip, ip_mask_24, nhop_pw)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(2):
            for num2 in range(2):
                vr_id = vr_list[num2]
                rif_uni_oid = rif_uni_oid_list[num][num2]
                uni_side_ip = uni_side_ip_list[num][num2]
                uni_nhop = uni_nhop_list[num][num2]
                mac_uni_peer = mac_uni_peer_list[num][num2]
                sai_thrift_remove_neighbor(self.client, addr_family, rif_uni_oid, uni_side_ip, mac_uni_peer)
                ret = sai_thrift_remove_route(self.client, vr_id, addr_family, uni_side_ip, ip_mask_24, uni_nhop)
                if (ret < 0):
                    sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
                self.client.sai_thrift_remove_next_hop(uni_nhop)
        for num in range(2):
            for num2 in range(2):
                rif_uni_oid = rif_uni_oid_list[num][num2]
                self.client.sai_thrift_remove_router_interface(rif_uni_oid)
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw3_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe3)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw3)
        
        inseg_entry = sai_thrift_inseg_entry_t(inseg_pw2_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_pw_pe1_to_pe2)
        self.client.sai_thrift_remove_tunnel(tunnel_id_pw2)
        
        self.client.sai_thrift_remove_router_interface(pw3_rif_oid)
        self.client.sai_thrift_remove_router_interface(pw2_rif_oid)
        
        self.client.sai_thrift_remove_virtual_router(pw3_vr_id)
        self.client.sai_thrift_remove_virtual_router(pw2_vr_id)
        
        inseg_entry = sai_thrift_inseg_entry_t(inseg_lsp_label) 
        self.client.sai_thrift_remove_inseg_entry(inseg_entry)
        self.client.sai_thrift_remove_next_hop(nhop_lsp_pe1_to_p)
        self.client.sai_thrift_remove_router_interface(mpls_rif_oid)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            nexthop_mac = provider_nexthop_mac_list[num]
            provider_rif_ip_addr = provider_rif_ip_addr_list[num]
            provider_nhop_oid = provider_nhop_oid_list[num]
            tmp_port = port_list[num]
            vlan_oid = provider_vlan_oid_list[num]
            nexthop_ip_addr = provider_nexthop_ip_addr_list[num]
            self.client.sai_thrift_remove_next_hop(provider_nhop_oid)
            sai_thrift_delete_fdb(self.client, vlan_oid, nexthop_mac, tmp_port)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, nexthop_ip_addr, ip_mask_32, provider_nhop_oid)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
            sai_thrift_remove_neighbor(self.client, addr_family, provider_rif_oid, nexthop_ip_addr, nexthop_mac)
            ret = sai_thrift_remove_route(self.client, default_vrf_oid, addr_family, provider_rif_ip_addr, ip_mask_32, 0)
            if (ret < 0):
                sys_logging(">>sai_thrift_remove_route failed!!! ret = %d" % ret)
        for num in range(1):
            provider_rif_oid = provider_rif_oid_list[num]
            self.client.sai_thrift_remove_router_interface(provider_rif_oid)
        for num in range(1):
            vlan_oid = provider_vlan_oid_list[num]
            vlan_id = provider_vlan_list[num]
            sai_thrift_vlan_remove_all_ports(self.client, vlan_oid)
            self.client.sai_thrift_remove_vlan(vlan_oid)