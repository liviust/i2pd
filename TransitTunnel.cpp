#include <string.h>
#include "I2PEndian.h"
#include "Log.h"
#include "RouterContext.h"
#include "I2NPProtocol.h"
#include "Tunnel.h"
#include "Transports.h"
#include "TransitTunnel.h"

namespace i2p
{
namespace tunnel
{	
	TransitTunnel::TransitTunnel (uint32_t receiveTunnelID, 
	    const uint8_t * nextIdent, uint32_t nextTunnelID, 
		const uint8_t * layerKey,const uint8_t * ivKey): 
			m_TunnelID (receiveTunnelID),  m_NextTunnelID (nextTunnelID), 
			m_NextIdent (nextIdent), m_NumTransmittedBytes (0)
	{	
		m_ECBEncryption.SetKey (ivKey); 
		m_CBCEncryption.SetKey (layerKey); 
	}	

	void TransitTunnel::EncryptTunnelMsg (I2NPMessage * tunnelMsg)
	{
		uint8_t * payload = tunnelMsg->GetPayload () + 4;
		
		m_ECBEncryption.Encrypt ((i2p::crypto::ChipherBlock *)payload, (i2p::crypto::ChipherBlock *)payload); // iv

		m_CBCEncryption.SetIV (payload); 
		m_CBCEncryption.Encrypt (payload + 16, TUNNEL_DATA_ENCRYPTED_SIZE, payload + 16); // payload

		m_ECBEncryption.Encrypt((i2p::crypto::ChipherBlock *)payload, (i2p::crypto::ChipherBlock *)payload); // double iv encryption
	}	
	
	void TransitTunnel::HandleTunnelDataMsg (i2p::I2NPMessage * tunnelMsg)
	{
		EncryptTunnelMsg (tunnelMsg);
		
		LogPrint ("TransitTunnel: ",m_TunnelID,"->", m_NextTunnelID);
		*(uint32_t *)(tunnelMsg->GetPayload ()) = htobe32 (m_NextTunnelID);
		FillI2NPMessageHeader (tunnelMsg, eI2NPTunnelData);
	
		i2p::transports.SendMessage (m_NextIdent, tunnelMsg);	
		m_NumTransmittedBytes += tunnelMsg->GetLength ();
	}

	void TransitTunnel::SendTunnelDataMsg (i2p::I2NPMessage * msg)
	{	
		LogPrint ("We are not a gateway for transit tunnel ", m_TunnelID);
		i2p::DeleteI2NPMessage (msg);	
	}		

	void TransitTunnelGateway::SendTunnelDataMsg (i2p::I2NPMessage * msg)
	{
		TunnelMessageBlock block;
		block.deliveryType = eDeliveryTypeLocal;
		block.data = msg;
		m_Gateway.SendTunnelDataMsg (block);
	}		

	void TransitTunnelEndpoint::HandleTunnelDataMsg (i2p::I2NPMessage * tunnelMsg)
	{
		EncryptTunnelMsg (tunnelMsg);
		
		LogPrint ("TransitTunnel endpoint for ", GetTunnelID ());
		m_Endpoint.HandleDecryptedTunnelDataMsg (tunnelMsg); 
	}
		
	TransitTunnel * CreateTransitTunnel (uint32_t receiveTunnelID,
		const uint8_t * nextIdent, uint32_t nextTunnelID, 
	    const uint8_t * layerKey,const uint8_t * ivKey, 
		bool isGateway, bool isEndpoint)
	{
		if (isEndpoint)
		{	
			LogPrint ("TransitTunnel endpoint: ", receiveTunnelID, " created");
			return new TransitTunnelEndpoint (receiveTunnelID, nextIdent, nextTunnelID, layerKey, ivKey);
		}	
		else if (isGateway)
		{	
			LogPrint ("TransitTunnel gateway: ", receiveTunnelID, " created");
			return new TransitTunnelGateway (receiveTunnelID, nextIdent, nextTunnelID, layerKey, ivKey);
		}	
		else	
		{	
			LogPrint ("TransitTunnel: ", receiveTunnelID, "->", nextTunnelID, " created");
			return new TransitTunnel (receiveTunnelID, nextIdent, nextTunnelID, layerKey, ivKey);
		}	
	}		
}
}