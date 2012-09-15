/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012 Icinga Development Team (http://www.icinga.org/)        *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "i2-compatido.h"

using namespace icinga;

/**
 * Returns the name of the component.
 *
 * @returns The name.
 */
String CompatIdoComponent::GetName(void) const
{
	return "compatido";
}

/**
 * Starts the component.
 */
void CompatIdoComponent::Start(void)
{
	/*
	 * TODO
	 * we should have configs for 
	 * - socket (unix or tcp)
	 * - data_processing_options
	 * - config_output_options - 0..orig, 1..retained (fake one)
	 * - INSTANCE_NAME ?
	 */
	m_StatusTimer = boost::make_shared<Timer>();
	m_StatusTimer->SetInterval(15);
	m_StatusTimer->OnTimerExpired.connect(boost::bind(&CompatIdoComponent::StatusTimerHandler, this));
	m_StatusTimer->Start();
	m_StatusTimer->Reschedule(0);

	m_ConfigTimer = boost::make_shared<Timer>();
	m_ConfigTimer->SetInterval(15);
	m_ConfigTimer->OnTimerExpired.connect(boost::bind(&CompatIdoComponent::ConfigTimerHandler, this));
	m_ConfigTimer->Start();
	m_ConfigTimer->Reschedule(0);

	/*
	 * open ido socket once, send the updates via timer then
	 */
	OpenSink("127.0.0.1", "5668");
	SendHello("i2-default");
}

/**
 * Stops the component.
 */
void CompatIdoComponent::Stop(void)
{

}

/* TODO
 * subscribe to all status updates and checkresults and dump them
 * should remove the periodic statusdata dump
 */

/**
 * Periodically dumps status information
 *
 * @param - Event arguments for the timer.
 */
void CompatIdoComponent::StatusTimerHandler(void)
{
	Logger::Write(LogInformation, "compatido", "TODO: Writing compat ido status information");
	/*
	 * TODO 
	 * - fetch status data, dump it periodically
	 * - subscribe to check events and status updates, dump it	
	 */
	DumpStatusData();

	/* 
	 * HINTS
	 * - we don't have any implizit host commands, fake it
	 */
}

/**
 * Periodically dumps config information
 *
 * @param - Event arguments for the timer.
 */
void CompatIdoComponent::ConfigTimerHandler(void)
{

	Logger::Write(LogInformation, "compatido", "TODO: Writing compat ido config updates information");
	/*
	 * TODO 
	 * - fetch config, dump it
	 * - subscribe to config update events, and send insert/update/delete for configs to ido2db
	 */

	DumpConfigObjects();

	/* 
	 * HINTS
	 * - we don't have any implizit host commands, fake it
	 */



}

/**
 * opens a tcp connection to the socket
 */
void CompatIdoComponent::OpenSink(String node, String service)
{
	m_IdoSocket = boost::make_shared<IdoSocket>(RoleOutbound);
	m_IdoSocket->Connect(node, service);
	m_IdoSocket->Start();
}

/**
 * sends hello msg to ido
 */
void CompatIdoComponent::SendHello(String instancename)
{
	time_t now;
	time(&now);

	/* IDO_API_CONNECTION is always TCP */
	/* IDO_API_CONNECTTYPE is always initial */
	stringstream message;
	message << "\n\n"
		<< IDO_API_HELLO << "\n"
		<< IDO_API_PROTOCOL << ": " << IDO_API_PROTOVERSION << "\n"
		<< IDO_API_AGENT << ": " << "I2 COMPAT-IDO" << "\n"
		<< IDO_API_AGENTVERSION << ": " << "2.0" << "\n"
		<< IDO_API_STARTTIME << ": " << now << "\n"
		<< IDO_API_DISPOSITION << ": " << IDO_API_DISPOSITION_REALTIME << "\n"
		<< IDO_API_CONNECTION << ": " << IDO_API_CONNECTION_TCPSOCKET << "\n"
		<< IDO_API_INSTANCENAME << ": " << instancename << "\n"
		<< IDO_API_STARTDATADUMP
		<< "\n\n";

	m_IdoSocket->SendMessage(message.str());
}

/**
 * sends config dump start signal to ido
 */
void CompatIdoComponent::StartConfigDump()
{
	struct timeval now;
	gettimeofday(&now, NULL);

	/* IDOMOD_CONFIG_DUMP_ORIGINAL=1 is the default config type */
	stringstream message;
	message << "\n\n"
		<< IDO_API_STARTCONFIGDUMP << ":" << "\n"
		<< IDO_DATA_CONFIGDUMPTYPE << "=" << 1 << "\n"
		<< IDO_DATA_TIMESTAMP << "=" << now.tv_sec << "." << now.tv_usec << "\n"
		<< IDO_API_ENDDATA
		<< "\n\n";

	m_IdoSocket->SendMessage(message.str());
}

/**
 * sends config dump end signal to ido
 */
void CompatIdoComponent::EndConfigDump()
{
        struct timeval now;
        gettimeofday(&now, NULL);

        stringstream message;
        message << "\n\n"
                << IDO_API_ENDCONFIGDUMP << ":" << "\n"
                << IDO_DATA_TIMESTAMP << "=" << now.tv_sec << "." << now.tv_usec << "\n"
                << IDO_API_ENDDATA
                << "\n\n";

        m_IdoSocket->SendMessage(message.str());
}

/**
 * dump host config to ido
 */
void CompatIdoComponent::DumpHostObject(const Host::Ptr& host)
{
        struct timeval now;
        gettimeofday(&now, NULL);

	stringstream message;
	message << "\n"
		<< IDO_API_HOSTDEFINITION << ":" << "\n"
		<< IDO_DATA_TIMESTAMP << "=" << now.tv_sec << "." << now.tv_usec << "\n"
		<< IDO_DATA_HOSTNAME << "=" << host->GetName() << "\n"	
		<< IDO_DATA_DISPLAYNAME << "=" << host->GetAlias() << "\n"	
		<< IDO_DATA_HOSTALIAS << "=" << host->GetAlias() << "\n"	
		<< IDO_DATA_HOSTADDRESS << "=" << "" << "\n"	
		<< IDO_DATA_HOSTADDRESS6 << "=" << "" << "\n"	
		<< IDO_DATA_HOSTCHECKCOMMAND << "=" << "" << "\n"	
		<< IDO_DATA_HOSTEVENTHANDLER << "=" << "" << "\n"	
		<< IDO_DATA_HOSTNOTIFICATIONPERIOD << "=" << "" << "\n"	
		<< IDO_DATA_HOSTCHECKPERIOD << "=" << "" << "\n"	
		<< IDO_DATA_HOSTFAILUREPREDICTIONOPTIONS << "=" << "" << "\n"	
		<< IDO_DATA_HOSTCHECKINTERVAL << "=" << 1 << "\n"	
		<< IDO_DATA_HOSTRETRYINTERVAL << "=" << 1 << "\n"	
		<< IDO_DATA_HOSTMAXCHECKATTEMPTS << "=" << 1 << "\n"	
		<< IDO_DATA_FIRSTNOTIFICATIONDELAY << "=" << 0 << "\n"	
		<< IDO_DATA_HOSTNOTIFICATIONINTERVAL << "=" << 0 << "\n"	
		<< IDO_DATA_NOTIFYHOSTDOWN << "=" << 0 << "\n"	
		<< IDO_DATA_NOTIFYHOSTUNREACHABLE << "=" << 0 << "\n"	
		<< IDO_DATA_NOTIFYHOSTRECOVERY << "=" << 0 << "\n"	
		<< IDO_DATA_NOTIFYHOSTFLAPPING << "=" << 0 << "\n"	
		<< IDO_DATA_NOTIFYHOSTDOWNTIME << "=" << 0 << "\n"	
		<< IDO_DATA_HOSTFLAPDETECTIONENABLED << "=" << 0 << "\n"	
		<< IDO_DATA_FLAPDETECTIONONUP << "=" << 0 << "\n"	
		<< IDO_DATA_FLAPDETECTIONOIDOWN << "=" << 0 << "\n"	
		<< IDO_DATA_FLAPDETECTIONONUNREACHABLE << "=" << 0 << "\n"	
		<< IDO_DATA_LOWHOSTFLAPTHRESHOLD << "=" << 0 << "\n"	
		<< IDO_DATA_HIGHHOSTFLAPTHRESHOLD << "=" << 0 << "\n"	
		<< IDO_DATA_STALKHOSTONUP << "=" << 0 << "\n"	
		<< IDO_DATA_STALKHOSTOIDOWN << "=" << 0 << "\n"	
		<< IDO_DATA_STALKHOSTONUNREACHABLE << "=" << 0 << "\n"	
		<< IDO_DATA_HOSTFRESHNESSCHECKSENABLED << "=" << 0 << "\n"	
		<< IDO_DATA_HOSTFRESHNESSTHRESHOLD << "=" << 0 << "\n"	
		<< IDO_DATA_PROCESSHOSTPERFORMANCEDATA << "=" << 1 << "\n"	
		<< IDO_DATA_ACTIVEHOSTCHECKSENABLED << "=" << 1 << "\n"	
		<< IDO_DATA_PASSIVEHOSTCHECKSENABLED << "=" << 1 << "\n"	
		<< IDO_DATA_HOSTEVENTHANDLERENABLED << "=" << 0 << "\n"	
		<< IDO_DATA_RETAINHOSTSTATUSINFORMATION << "=" << 1 << "\n"	
		<< IDO_DATA_RETAINHOSTNONSTATUSINFORMATION << "=" << 1 << "\n"	
		<< IDO_DATA_HOSTNOTIFICATIONSENABLED << "=" << 1 << "\n"	
		<< IDO_DATA_HOSTFAILUREPREDICTIONENABLED << "=" << 0 << "\n"	
		<< IDO_DATA_OBSESSOVERHOST << "=" << 0 << "\n"	
		<< IDO_DATA_NOTES << "=" << "i2_notes" << "\n"	
		<< IDO_DATA_NOTESURL << "=" << "" << "\n"	
		<< IDO_DATA_ACTIONURL << "=" << "" << "\n"	
		<< IDO_DATA_ICONIMAGE << "=" << "" << "\n"	
		<< IDO_DATA_ICONIMAGEALT << "=" << "" << "\n"	
		<< IDO_DATA_VRMLIMAGE << "=" << "" << "\n"	
		<< IDO_DATA_STATUSMAPIMAGE << "=" << "" << "\n"	
		<< IDO_DATA_HAVE2DCOORDS << "=" << 0 << "\n"	
		<< IDO_DATA_X2D << "=" << 0.0 << "\n"	
		<< IDO_DATA_Y2D << "=" << 0.0 << "\n"	
		<< IDO_DATA_HAVE3DCOORDS << "=" << 0 << "\n"	
		<< IDO_DATA_X3D << "=" << 0.0 << "\n"	
		<< IDO_DATA_Y3D << "=" << 0.0 << "\n"	
		<< IDO_DATA_Z3D<< "=" << 0.0 << "\n";
		/* FIXME add more related config items
	 	* parents, contactgroups, contacts, custom vars
	 	* before sending the message
	 	*/


        m_IdoSocket->SendMessage(message.str());
}

/**
 * dump host status to ido
 */
void CompatIdoComponent::DumpHostStatus(const Host::Ptr& host)
{

	//FIXME
}

/**
 * dump service config to ido
 */
void CompatIdoComponent::DumpServiceObject(const Service::Ptr& service)
{
        struct timeval now;
        gettimeofday(&now, NULL);

	stringstream message;
	message << "\n"
		<< IDO_API_SERVICEDEFINITION << ":" << "\n"
		<< IDO_DATA_TIMESTAMP << "=" << now.tv_sec << "." << now.tv_usec << "\n"
		<< IDO_DATA_HOSTNAME << "=" << service->GetHost()->GetName() << "\n"
		<< IDO_DATA_DISPLAYNAME  << "=" << service->GetAlias() << "\n"
		<< IDO_DATA_SERVICEDESCRIPTION << "=" << service->GetAlias() << "\n"
		<< IDO_DATA_SERVICECHECKCOMMAND << "=" << "check_i2" << "\n"
		<< IDO_DATA_SERVICEEVENTHANDLER << "=" << "" << "\n"
		<< IDO_DATA_SERVICENOTIFICATIONPERIOD << "=" << "" << "\n"
		<< IDO_DATA_SERVICECHECKPERIOD << "=" << "" << "\n"
		<< IDO_DATA_SERVICEFAILUREPREDICTIONOPTIONS << "=" << "" << "\n"
		<< IDO_DATA_SERVICECHECKINTERVAL << "=" << 1 << "\n"
		<< IDO_DATA_SERVICERETRYINTERVAL << "=" << 1 << "\n"
		<< IDO_DATA_MAXSERVICECHECKATTEMPTS << "=" << 1 << "\n"
		<< IDO_DATA_FIRSTNOTIFICATIONDELAY << "=" << 0 << "\n"
		<< IDO_DATA_SERVICENOTIFICATIONINTERVAL << "=" << 0 << "\n"
		<< IDO_DATA_NOTIFYSERVICEUNKNOWN << "=" << 0 << "\n"
		<< IDO_DATA_NOTIFYSERVICEWARNING << "=" << 0 << "\n"
		<< IDO_DATA_NOTIFYSERVICECRITICAL << "=" << 0 << "\n"
		<< IDO_DATA_NOTIFYSERVICERECOVERY << "=" << 0 << "\n"
		<< IDO_DATA_NOTIFYSERVICEFLAPPING << "=" << 0 << "\n"
		<< IDO_DATA_NOTIFYSERVICEDOWNTIME << "=" << 0 << "\n"
		<< IDO_DATA_STALKSERVICEONOK << "=" << 0 << "\n"
		<< IDO_DATA_STALKSERVICEONWARNING << "=" << 0 << "\n"
		<< IDO_DATA_STALKSERVICEONUNKNOWN << "=" << 0 << "\n"
		<< IDO_DATA_STALKSERVICEONCRITICAL << "=" << 0 << "\n"
		<< IDO_DATA_SERVICEISVOLATILE << "=" << 0 << "\n"
		<< IDO_DATA_SERVICEFLAPDETECTIONENABLED << "=" << 0 << "\n"
		<< IDO_DATA_FLAPDETECTIONONOK << "=" << 0 << "\n"
		<< IDO_DATA_FLAPDETECTIONONWARNING << "=" << 0 << "\n"
		<< IDO_DATA_FLAPDETECTIONONUNKNOWN << "=" << 0 << "\n"
		<< IDO_DATA_FLAPDETECTIONONCRITICAL << "=" << 0 << "\n"
		<< IDO_DATA_LOWSERVICEFLAPTHRESHOLD << "=" << 0 << "\n"
		<< IDO_DATA_HIGHSERVICEFLAPTHRESHOLD << "=" << 0 << "\n"
		<< IDO_DATA_PROCESSSERVICEPERFORMANCEDATA << "=" << 1 << "\n"
		<< IDO_DATA_SERVICEFRESHNESSCHECKSENABLED << "=" << 0 << "\n"
		<< IDO_DATA_SERVICEFRESHNESSTHRESHOLD << "=" << 0 << "\n"
		<< IDO_DATA_PASSIVESERVICECHECKSENABLED << "=" << 1 << "\n"
		<< IDO_DATA_SERVICEEVENTHANDLERENABLED << "=" << 0 << "\n"
		<< IDO_DATA_ACTIVESERVICECHECKSENABLED << "=" << 1 << "\n"
		<< IDO_DATA_RETAINSERVICESTATUSINFORMATION << "=" << 1 << "\n"
		<< IDO_DATA_RETAINSERVICENONSTATUSINFORMATION << "=" << 1 << "\n"
		<< IDO_DATA_SERVICENOTIFICATIONSENABLED << "=" << 0 << "\n"
		<< IDO_DATA_OBSESSOVERSERVICE << "=" << 0 << "\n"
		<< IDO_DATA_SERVICEFAILUREPREDICTIONENABLED << "=" << 0 << "\n"
		<< IDO_DATA_NOTES << "=" << "i2_notes" << "\n"
		<< IDO_DATA_NOTESURL << "=" << "" << "\n"
		<< IDO_DATA_ACTIONURL << "=" << "" << "\n"
		<< IDO_DATA_ICONIMAGE << "=" << "" << "\n"
		<< IDO_DATA_ICONIMAGEALT << "=" << "" << "\n";
		/* FIXME add more related config items
	 	* contactgroups, contacts, custom vars
	 	* before sending the message
	 	*/

	Logger::Write(LogInformation, "compatido", "Writing compat ido service");
        m_IdoSocket->SendMessage(message.str());
}

/**
 * dump service status to ido
 */ 
void CompatIdoComponent::DumpServiceStatus(const Service::Ptr& service)
{

	//FIXME
}

/**
 * process and dump all config objects
 */
void CompatIdoComponent::DumpConfigObjects(void)
{
	/* TODO dump
	 * commands, timeperiods, contact*
	 * escalations, dependencies
	 * if needed/available.
	 */
	Logger::Write(LogInformation, "compatido", "Writing compat ido config information");

	/* tell ido2db that we start now */
	StartConfigDump();

	/* hosts and hostgroups */
	map<String, vector<String> > hostgroups;

	DynamicObject::Ptr object;
	BOOST_FOREACH(tie(tuples::ignore, object), DynamicObject::GetObjects("Host")) {
		const Host::Ptr& host = static_pointer_cast<Host>(object);

		Dictionary::Ptr dict;
		dict = host->GetGroups();

		if (dict) {
			Value hostgroup;
			BOOST_FOREACH(tie(tuples::ignore, hostgroup), dict) {
				hostgroups[hostgroup].push_back(host->GetName());
			}
		}

		DumpHostObject(host);
	}

	pair<String, vector<String > > hgt;
	BOOST_FOREACH(hgt, hostgroups) {
		const String& name = hgt.first;
		const vector<String>& hosts = hgt.second;

		if(HostGroup::Exists(name)) {
			HostGroup::Ptr hg = HostGroup::GetByName(name);

			/* dump the hostgroup and its attributes/members to ido */
        		struct timeval now;
        		gettimeofday(&now, NULL);

			stringstream message;
			message << "\n"
				<< IDO_API_HOSTGROUPDEFINITION << ":" << "\n"
				<< IDO_DATA_TIMESTAMP << "=" << now.tv_sec << "." << now.tv_usec << "\n"
				<< IDO_DATA_HOSTGROUPNAME << "=" << name << "\n"
				<< IDO_DATA_HOSTGROUPALIAS << "=" << hg->GetAlias() << "\n";

			CreateMessageList(message, hosts, IDO_DATA_HOSTGROUPMEMBER);
			//vector<String>::iterator it;
			//for (it = hosts.begin(); it != hosts.end(); it++) {
			//	message << IDO_DATA_HOSTGROUPMEMBER << "=" << *it << "\n";
			//}
				
			message << IDO_API_ENDDATA << "\n\n";

        		m_IdoSocket->SendMessage(message.str());
		}
	}

	/* services and servicegroups */
	map<String, vector<Service::Ptr> > servicegroups;

	BOOST_FOREACH(tie(tuples::ignore, object), DynamicObject::GetObjects("Service")) {
		Service::Ptr service = static_pointer_cast<Service>(object);

		Dictionary::Ptr dict;

		dict = service->GetGroups();

		if (dict) {
			Value servicegroup;
			BOOST_FOREACH(tie(tuples::ignore, servicegroup), dict) {
				servicegroups[servicegroup].push_back(service);
			}
		}

		DumpServiceObject(service);
	}

	pair<String, vector<Service::Ptr> > sgt;
	BOOST_FOREACH(sgt, servicegroups) {
		const String& name = sgt.first;
		const vector<Service::Ptr>& services = sgt.second;

		if (ServiceGroup::Exists(name)) {
			ServiceGroup::Ptr sg = ServiceGroup::GetByName(name);

			/* dump the servicegroup and its attributes/members to ido */
                        struct timeval now;
                        gettimeofday(&now, NULL);

                        stringstream message;
                        message << "\n"
                                << IDO_API_SERVICEGROUPDEFINITION << ":" << "\n"
                                << IDO_DATA_TIMESTAMP << "=" << now.tv_sec << "." << now.tv_usec << "\n"
                                << IDO_DATA_SERVICEGROUPNAME << "=" << name << "\n"
                                << IDO_DATA_SERVICEGROUPALIAS << "=" << sg->GetAlias() << "\n";

			vector<String> sglist;
			vector<Service::Ptr>::iterator vt;

			BOOST_FOREACH(const Service::Ptr& service, services) {
				sglist.push_back(service->GetHost()->GetName());
				sglist.push_back(service->GetAlias());
			}
	
			CreateMessageList(message, services, IDO_DATA_SERVICEGROUPMEMBER);
                        //for (vt = sglist.begin(); vt != sglist.end(); vt++) { 
                        //        message << IDO_DATA_SERVICEGROUPMEMBER << "=" << *vt << "\n";
                        //}       

                        message << IDO_API_ENDDATA << "\n\n";

        		m_IdoSocket->SendMessage(message.str());
		}
	}

	/* tell ido2db that we ended dumping the config */
	EndConfigDump();
}

/**
 * process and dump all status data 
 */
void CompatIdoComponent::DumpStatusData(void)
{
	Logger::Write(LogInformation, "compatido", "Writing compat ido status information");

	stringstream message;

	/* program status - should probably done more often */
	message << "\n\n";

}



EXPORT_COMPONENT(compatido, CompatIdoComponent);
