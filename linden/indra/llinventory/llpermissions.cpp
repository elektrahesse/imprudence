/** 
 * @file llpermissions.cpp
 * @author Phoenix
 * @brief Permissions for objects and inventory.
 *
 * Copyright (c) 2002-2007, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 */

#include "linden_common.h"

#include "llpermissions.h"

// library includes
#include "message.h"
#include "metapropertyt.h"

///----------------------------------------------------------------------------
/// Class LLPermissions
///----------------------------------------------------------------------------

const LLPermissions LLPermissions::DEFAULT;

// No creator = created by system
LLPermissions::LLPermissions()
{
	init(LLUUID::null, LLUUID::null, LLUUID::null, LLUUID::null);
}


// Default to created by system
void LLPermissions::init(const LLUUID& creator, const LLUUID& owner, const LLUUID& last_owner, const LLUUID& group)
{
	mCreator	= creator;
	mOwner		= owner;
	mLastOwner	= last_owner;
	mGroup		= group;

	mMaskBase		= PERM_ALL;
	mMaskOwner		= PERM_ALL;
	mMaskEveryone	= PERM_ALL;
	mMaskGroup		= PERM_ALL;
	mMaskNextOwner = PERM_ALL;
	fixOwnership();
}


void LLPermissions::initMasks(PermissionMask base, PermissionMask owner,
							  PermissionMask everyone, PermissionMask group,
							  PermissionMask next)
{
	mMaskBase		= base;
	mMaskOwner		= owner;
	mMaskEveryone	= everyone;
	mMaskGroup		= group;
	mMaskNextOwner = next;
	fixFairUse();
	fix();
}

BOOL LLPermissions::getOwnership(LLUUID& owner_id, BOOL& is_group_owned) const
{
	if(mOwner.notNull())
	{
		owner_id = mOwner;
		is_group_owned = FALSE;
		return TRUE;
	}
	else if(mIsGroupOwned)
	{
		owner_id = mGroup;
		is_group_owned = TRUE;
		return TRUE;
	}
	return FALSE;
}

LLUUID LLPermissions::getSafeOwner() const
{
	if(mOwner.notNull())
	{
		return mOwner;
	}
	else if(mIsGroupOwned)
	{
		return mGroup;
	}
	else
	{
		llwarns << "LLPermissions::getSafeOwner() called with no valid owner!" << llendl;
		LLUUID unused_uuid;
		unused_uuid.generate();

		return unused_uuid;
	}
}

U32 LLPermissions::getCRC32() const
{
	U32 rv = mCreator.getCRC32();
	rv += mOwner.getCRC32();
	rv += mLastOwner.getCRC32();
	rv += mGroup.getCRC32();
	rv += mMaskBase + mMaskOwner + mMaskEveryone + mMaskGroup;
	return rv;
}

void LLPermissions::set(const LLPermissions& from)
{
	mCreator	= from.mCreator;
	mOwner		= from.mOwner;
	mLastOwner	= from.mLastOwner;
	mGroup		= from.mGroup;

	mMaskBase		= from.mMaskBase;
	mMaskOwner		= from.mMaskOwner;
	mMaskEveryone	= from.mMaskEveryone;
	mMaskGroup		= from.mMaskGroup;
	mMaskNextOwner = from.mMaskNextOwner;
	mIsGroupOwned = from.mIsGroupOwned;
}

// Fix hierarchy of permissions. 
void LLPermissions::fix()
{
	mMaskOwner &= mMaskBase;
	mMaskGroup &= mMaskOwner;
	// next owner uses base, since you may want to sell locked objects.
	mMaskNextOwner &= mMaskBase;
	mMaskEveryone &= mMaskOwner;
	mMaskEveryone &= ~PERM_MODIFY;
	if(!(mMaskBase & PERM_TRANSFER) && !mIsGroupOwned)
	{
		mMaskGroup &= ~PERM_COPY;
		mMaskEveryone &= ~PERM_COPY;
		// Do not set mask next owner to too restrictive because if we
		// rez an object, it may require an ownership transfer during
		// rez, which will note the overly restrictive perms, and then
		// fix them to allow fair use, which may be different than the
		// original intention.
	}
}

// Correct for fair use - you can never take away the right to move
// stuff you own, and you can never take away the right to transfer
// something you cannot otherwise copy.
void LLPermissions::fixFairUse()
{
	mMaskBase |= PERM_MOVE;
	if(!(mMaskBase & PERM_COPY))
	{
		mMaskBase |= PERM_TRANSFER;
	}
	// (mask next owner == PERM_NONE) iff mask base is no transfer
	if(mMaskNextOwner != PERM_NONE)
	{
		mMaskNextOwner |= PERM_MOVE;
	}
}

void LLPermissions::fixOwnership()
{
	if(mOwner.isNull() && mGroup.notNull())
	{
		mIsGroupOwned = true;
	}
	else
	{
		mIsGroupOwned = false;
	}
}

// Allow accumulation of permissions. Results in the tightest
// permissions possible. In the case of clashing UUIDs, it sets the ID
// to LLUUID::null.
void LLPermissions::accumulate(const LLPermissions& perm)
{
	if(perm.mCreator != mCreator)
	{
		mCreator = LLUUID::null;
	}
	if(perm.mOwner != mOwner)
	{
		mOwner = LLUUID::null;
	}
	if(perm.mLastOwner != mLastOwner)
	{
		mLastOwner = LLUUID::null;
	}
	if(perm.mGroup != mGroup)
	{
		mGroup = LLUUID::null;
	}

	mMaskBase &= perm.mMaskBase;
	mMaskOwner &= perm.mMaskOwner;
	mMaskGroup &= perm.mMaskGroup;
	mMaskEveryone &= perm.mMaskEveryone;
	mMaskNextOwner &= perm.mMaskNextOwner;
	fix();
}

// saves last owner, sets current owner, and sets the group.  note
// that this function has to more cleverly apply the fair use
// permissions.
BOOL LLPermissions::setOwnerAndGroup(
	const LLUUID& agent,
	const LLUUID& owner,
	const LLUUID& group,
	bool is_atomic)
{
	BOOL allowed = FALSE;

	if( agent.isNull() || mOwner.isNull()
	   || ((agent == mOwner) && ((owner == mOwner) || (mMaskOwner & PERM_TRANSFER)) ) )
	{
		// ...system can alway set owner
		// ...public objects can be claimed by anyone
		// ...otherwise, agent must own it and have transfer ability
		allowed = TRUE;
	}

	if (allowed)
	{
		if(mLastOwner.isNull() || (!mOwner.isNull() && (owner != mLastOwner)))
		{
			mLastOwner = mOwner;
		}
		if((mOwner != owner)
		   || (mOwner.isNull() && owner.isNull() && (mGroup != group)))
		{
			mMaskBase = mMaskNextOwner;
			mOwner = owner;
			// this is a selective use of fair use for atomic
			// permissions.
			if(is_atomic && !(mMaskBase & PERM_COPY))
			{
				mMaskBase |= PERM_TRANSFER;
			}
		}
		mGroup = group;
		fixOwnership();
		// if it's not atomic and we fix fair use, it blows away
		//objects as inventory items which have different permissions
		//than it's contents. :(
		// fixFairUse();
		mMaskBase |= PERM_MOVE;
		if(mMaskNextOwner != PERM_NONE) mMaskNextOwner |= PERM_MOVE;
		fix();
	}

	return allowed;
}

BOOL LLPermissions::deedToGroup(const LLUUID& agent, const LLUUID& group)
{
	if(group.notNull() && (agent.isNull() || ((group == mGroup)
											  && (mMaskOwner & PERM_TRANSFER)
											  && (mMaskGroup & PERM_MOVE))))
	{
		if(mOwner.notNull())
		{
			mLastOwner = mOwner;
			mOwner.setNull();
		}
		mMaskBase = mMaskNextOwner;
		mMaskGroup = PERM_NONE;
		mGroup = group;
		mIsGroupOwned = true;
		fixFairUse();
		fix();
		return TRUE;
	}
	return FALSE;
}

BOOL LLPermissions::setBaseBits(const LLUUID& agent, BOOL set, PermissionMask bits)
{
	BOOL ownership = FALSE;
	if(agent.isNull())
	{
		// only the system is always allowed to change base bits
		ownership = TRUE;
	}

	if (ownership)
	{
		if (set)
		{
			mMaskBase |= bits;		// turn on bits
		}
		else
		{
			mMaskBase &= ~bits;		// turn off bits
		}
		fix();
	}

	return ownership;
}


// Note: If you attempt to set bits that the base bits doesn't allow,
// the function will succeed, but those bits will not be set.
BOOL LLPermissions::setOwnerBits(const LLUUID& agent, BOOL set, PermissionMask bits)
{
	BOOL ownership = FALSE;

	if(agent.isNull())
	{
		// ...system always allowed to change things
		ownership = TRUE;
	}
	else if (agent == mOwner)
	{
		// ...owner bits can only be set by owner
		ownership = TRUE;
	}

	// If we have correct ownership and 
	if (ownership)
	{
		if (set)
		{
			mMaskOwner |= bits;			// turn on bits
		}
		else
		{
			mMaskOwner &= ~bits;		// turn off bits
		}
		fix();
	}

	return (ownership);
}

BOOL LLPermissions::setGroupBits(const LLUUID& agent, const LLUUID& group, BOOL set, PermissionMask bits)
{
	BOOL ownership = FALSE;
	if((agent.isNull()) || (agent == mOwner)
	   || ((group == mGroup) && (!mGroup.isNull())))
	{
		// The group bits can be set by the system, the owner, or a
		// group member.
		ownership = TRUE;
	}

	if (ownership)
	{
		if (set)
		{
			mMaskGroup |= bits;
		}
		else
		{
			mMaskGroup &= ~bits;
		}
		fix();
	}
	return ownership;
}


// Note: If you attempt to set bits that the creator or owner doesn't allow,
// the function will succeed, but those bits will not be set.
BOOL LLPermissions::setEveryoneBits(const LLUUID& agent, const LLUUID& group, BOOL set, PermissionMask bits)
{
	BOOL ownership = FALSE;
	if((agent.isNull()) || (agent == mOwner)
	   || ((group == mGroup) && (!mGroup.isNull())))
	{
		// The everyone bits can be set by the system, the owner, or a
		// group member.
		ownership = TRUE;
	}
	if (ownership)
	{
		if (set)
		{
			mMaskEveryone |= bits;
		}
		else
		{
			mMaskEveryone &= ~bits;
		}

		// Fix hierarchy of permissions
		fix();
	}
	return ownership;
}

// Note: If you attempt to set bits that the creator or owner doesn't allow,
// the function will succeed, but those bits will not be set.
BOOL LLPermissions::setNextOwnerBits(const LLUUID& agent, const LLUUID& group, BOOL set, PermissionMask bits)
{
	BOOL ownership = FALSE;
	if((agent.isNull()) || (agent == mOwner)
	   || ((group == mGroup) && (!mGroup.isNull())))
	{
		// The next owner bits can be set by the system, the owner, or
		// a group member.
		ownership = TRUE;
	}
	if (ownership)
	{
		if (set)
		{
			mMaskNextOwner |= bits;
		}
		else
		{
			mMaskNextOwner &= ~bits;
		}

		// Fix-up permissions
		if(!(mMaskNextOwner & PERM_COPY))
		{
			mMaskNextOwner |= PERM_TRANSFER;
		}
		fix();
	}
	return ownership;
}

BOOL LLPermissions::allowOperationBy(PermissionBit op, const LLUUID& requester, const LLUUID& group) const
{
	if(requester.isNull())
	{
		// ...system making request
		// ...not owned
		return TRUE;
	}
	else if (mIsGroupOwned && (mGroup == requester))
	{
		// group checking ownership permissions
		return (mMaskOwner & op);
	}
	else if (!mIsGroupOwned && (mOwner == requester))
	{
		// ...owner making request
		return (mMaskOwner & op);
	}
	else if(mGroup.notNull() && (mGroup == group))
	{
		// group member making request
		return ((mMaskGroup & op) || (mMaskEveryone & op));
	}
	return (mMaskEveryone & op);
}

//
// Messaging support
//
void LLPermissions::packMessage(LLMessageSystem* msg) const
{
	msg->addUUIDFast(_PREHASH_CreatorID, mCreator);
	msg->addUUIDFast(_PREHASH_OwnerID, mOwner);
	msg->addUUIDFast(_PREHASH_GroupID, mGroup);

	msg->addU32Fast(_PREHASH_BaseMask,		mMaskBase );
	msg->addU32Fast(_PREHASH_OwnerMask,	mMaskOwner );
	msg->addU32Fast(_PREHASH_GroupMask,	mMaskGroup );
	msg->addU32Fast(_PREHASH_EveryoneMask,	mMaskEveryone );
	msg->addU32Fast(_PREHASH_NextOwnerMask, mMaskNextOwner );
	msg->addBOOLFast(_PREHASH_GroupOwned, (BOOL)mIsGroupOwned);
}


void LLPermissions::unpackMessage(LLMessageSystem* msg, const char* block, S32 block_num)
{
	msg->getUUIDFast(block, _PREHASH_CreatorID,	mCreator, block_num);
	msg->getUUIDFast(block, _PREHASH_OwnerID,	mOwner, block_num);
	msg->getUUIDFast(block, _PREHASH_GroupID,	mGroup, block_num);

	msg->getU32Fast(block, _PREHASH_BaseMask, mMaskBase, block_num );
	msg->getU32Fast(block, _PREHASH_OwnerMask, mMaskOwner, block_num );
	msg->getU32Fast(block, _PREHASH_GroupMask, mMaskGroup, block_num );
	msg->getU32Fast(block, _PREHASH_EveryoneMask, mMaskEveryone, block_num );
	msg->getU32Fast(block, _PREHASH_NextOwnerMask, mMaskNextOwner, block_num );
	BOOL tmp;
	msg->getBOOLFast(block, _PREHASH_GroupOwned, tmp, block_num);
	mIsGroupOwned = (bool)tmp;
}


//
// File support
//

BOOL LLPermissions::importFile(FILE* fp)
{
	init(LLUUID::null, LLUUID::null, LLUUID::null, LLUUID::null);
	const S32 BUFSIZE = 16384;

	// *NOTE: Changing the buffer size will require changing the scanf
	// calls below.
	char buffer[BUFSIZE];	/* Flawfinder: ignore */
	char keyword[256];	/* Flawfinder: ignore */
	char valuestr[256];	/* Flawfinder: ignore */
	char uuid_str[256];	/* Flawfinder: ignore */
	U32 mask;

	keyword[0]  = '\0';
	valuestr[0] = '\0';

	while (!feof(fp))
	{
		fgets(buffer, BUFSIZE, fp);
		sscanf( /* Flawfinder: ignore */
			buffer,
			" %255s %255s",
			keyword, valuestr);
		if (!keyword)
		{
			continue;
		}
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}",keyword))
		{
			break;
		}
		else if (!strcmp("creator_mask", keyword))
		{
			// legacy support for "creator" masks
			sscanf(valuestr, "%x", &mask);
			mMaskBase = mask;
			fixFairUse();
		}
		else if (!strcmp("base_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskBase = mask;
			//fixFairUse();
		}
		else if (!strcmp("owner_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskOwner = mask;
		}
		else if (!strcmp("group_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskGroup = mask;
		}
		else if (!strcmp("everyone_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskEveryone = mask;
		}
		else if (!strcmp("next_owner_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskNextOwner = mask;
		}
		else if (!strcmp("creator_id", keyword))
		{
			sscanf(valuestr, "%255s", uuid_str); /* Flawfinder: ignore */
			mCreator.set(uuid_str);
		}
		else if (!strcmp("owner_id", keyword))
		{
			sscanf(valuestr, "%255s", uuid_str); /* Flawfinder: ignore */
			mOwner.set(uuid_str);
		}
		else if (!strcmp("last_owner_id", keyword))
		{
			sscanf(valuestr, "%255s", uuid_str); /* Flawfinder: ignore */
			mLastOwner.set(uuid_str);
		}
		else if (!strcmp("group_id", keyword))
		{
			sscanf(valuestr, "%255s", uuid_str); /* Flawfinder: ignore */
			mGroup.set(uuid_str);
		}
		else if (!strcmp("group_owned", keyword))
		{
			sscanf(valuestr, "%d", &mask);
			if(mask) mIsGroupOwned = true;
			else mIsGroupOwned = false;
		}
		else
		{
			llinfos << "unknown keyword " << keyword << " in permissions import" << llendl;
		}
	}
	fix();
	return TRUE;
}


BOOL LLPermissions::exportFile(FILE* fp) const
{
	char uuid_str[256];	/* Flawfinder: ignore */

	fprintf(fp, "\tpermissions 0\n");
	fprintf(fp, "\t{\n");

	fprintf(fp, "\t\tbase_mask\t%08x\n",		mMaskBase);
	fprintf(fp, "\t\towner_mask\t%08x\n",		mMaskOwner);
	fprintf(fp, "\t\tgroup_mask\t%08x\n",		mMaskGroup);
	fprintf(fp, "\t\teveryone_mask\t%08x\n",	mMaskEveryone);
	fprintf(fp, "\t\tnext_owner_mask\t%08x\n",	mMaskNextOwner);

	mCreator.toString(uuid_str);
	fprintf(fp, "\t\tcreator_id\t%s\n",			uuid_str);

	mOwner.toString(uuid_str);
	fprintf(fp, "\t\towner_id\t%s\n",			uuid_str);

	mLastOwner.toString(uuid_str);
	fprintf(fp, "\t\tlast_owner_id\t%s\n",		uuid_str);

	mGroup.toString(uuid_str);
	fprintf(fp, "\t\tgroup_id\t%s\n",			uuid_str);

	if(mIsGroupOwned)
	{
		fprintf(fp, "\t\tgroup_owned\t1\n");
	}
	fprintf(fp,"\t}\n");
	return TRUE;
}


BOOL LLPermissions::importLegacyStream(std::istream& input_stream)
{
	init(LLUUID::null, LLUUID::null, LLUUID::null, LLUUID::null);
	const S32 BUFSIZE = 16384;

	// *NOTE: Changing the buffer size will require changing the scanf
	// calls below.
	char buffer[BUFSIZE];	/* Flawfinder: ignore */
	char keyword[256];	/* Flawfinder: ignore */
	char valuestr[256];	/* Flawfinder: ignore */
	char uuid_str[256];	/* Flawfinder: ignore */
	U32 mask;

	keyword[0]  = '\0';
	valuestr[0] = '\0';

	while (input_stream.good())
	{
		input_stream.getline(buffer, BUFSIZE);
		sscanf( /* Flawfinder: ignore */
			buffer,
			" %255s %255s",
			keyword, valuestr);
		if (!keyword)
		{
			continue;
		}
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}",keyword))
		{
			break;
		}
		else if (!strcmp("creator_mask", keyword))
		{
			// legacy support for "creator" masks
			sscanf(valuestr, "%x", &mask);
			mMaskBase = mask;
			fixFairUse();
		}
		else if (!strcmp("base_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskBase = mask;
			//fixFairUse();
		}
		else if (!strcmp("owner_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskOwner = mask;
		}
		else if (!strcmp("group_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskGroup = mask;
		}
		else if (!strcmp("everyone_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskEveryone = mask;
		}
		else if (!strcmp("next_owner_mask", keyword))
		{
			sscanf(valuestr, "%x", &mask);
			mMaskNextOwner = mask;
		}
		else if (!strcmp("creator_id", keyword))
		{
			sscanf(valuestr, "%255s", uuid_str);	/* Flawfinder: ignore */
			mCreator.set(uuid_str);
		}
		else if (!strcmp("owner_id", keyword))
		{
			sscanf(valuestr, "%255s", uuid_str);	/* Flawfinder: ignore */
			mOwner.set(uuid_str);
		}
		else if (!strcmp("last_owner_id", keyword))
		{
			sscanf(valuestr, "%255s", uuid_str);	/* Flawfinder: ignore */
			mLastOwner.set(uuid_str);
		}
		else if (!strcmp("group_id", keyword))
		{
			sscanf(valuestr, "%255s", uuid_str);	/* Flawfinder: ignore */
			mGroup.set(uuid_str);
		}
		else if (!strcmp("group_owned", keyword))
		{
			sscanf(valuestr, "%d", &mask);
			if(mask) mIsGroupOwned = true;
			else mIsGroupOwned = false;
		}
		else
		{
			llinfos << "unknown keyword " << keyword << " in permissions import" << llendl;
		}
	}
	fix();
	return TRUE;
}


BOOL LLPermissions::exportLegacyStream(std::ostream& output_stream) const
{
	char uuid_str[256];	/* Flawfinder: ignore */

	output_stream <<  "\tpermissions 0\n";
	output_stream <<  "\t{\n";

	char buffer[256];	/* Flawfinder: ignore */
	snprintf(buffer, sizeof(buffer), "\t\tbase_mask\t%08x\n",		mMaskBase);		/* Flawfinder: ignore */
	output_stream << buffer;
	snprintf(buffer, sizeof(buffer), "\t\towner_mask\t%08x\n",		mMaskOwner);		/* Flawfinder: ignore */
	output_stream << buffer;
	snprintf(buffer, sizeof(buffer), "\t\tgroup_mask\t%08x\n",		mMaskGroup);		/* Flawfinder: ignore */
	output_stream << buffer;
	snprintf(buffer, sizeof(buffer), "\t\teveryone_mask\t%08x\n",	mMaskEveryone);		/* Flawfinder: ignore */
	output_stream << buffer;
	snprintf(buffer, sizeof(buffer), "\t\tnext_owner_mask\t%08x\n",	mMaskNextOwner);		/* Flawfinder: ignore */
	output_stream << buffer;

	mCreator.toString(uuid_str);
	output_stream <<  "\t\tcreator_id\t" << uuid_str << "\n";

	mOwner.toString(uuid_str);
	output_stream <<  "\t\towner_id\t" << uuid_str << "\n";

	mLastOwner.toString(uuid_str);
	output_stream <<  "\t\tlast_owner_id\t" << uuid_str << "\n";

	mGroup.toString(uuid_str);
	output_stream <<  "\t\tgroup_id\t" << uuid_str << "\n";

	if(mIsGroupOwned)
	{
		output_stream <<  "\t\tgroup_owned\t1\n";
	}
	output_stream << "\t}\n";
	return TRUE;
}


LLXMLNode *LLPermissions::exportFileXML() const
{
	LLXMLNode *ret = new LLXMLNode("permissions", FALSE);

	ret->createChild("group_owned", TRUE)->setBoolValue(1, (const BOOL*)&mIsGroupOwned);

	ret->createChild("base_mask", FALSE)->setByteValue(4, (U8*)&mMaskBase, LLXMLNode::ENCODING_HEX);
	ret->createChild("owner_mask", FALSE)->setByteValue(4, (U8*)&mMaskOwner, LLXMLNode::ENCODING_HEX);
	ret->createChild("group_mask", FALSE)->setByteValue(4, (U8*)&mMaskGroup, LLXMLNode::ENCODING_HEX);
	ret->createChild("everyone_mask", FALSE)->setByteValue(4, (U8*)&mMaskEveryone, LLXMLNode::ENCODING_HEX);
	ret->createChild("next_owner_mask", FALSE)->setByteValue(4, (U8*)&mMaskNextOwner, LLXMLNode::ENCODING_HEX);

	ret->createChild("creator_id", FALSE)->setUUIDValue(1, &mCreator);
	ret->createChild("owner_id", FALSE)->setUUIDValue(1, &mOwner);
	ret->createChild("last_owner_id", FALSE)->setUUIDValue(1, &mLastOwner);
	ret->createChild("group_id", FALSE)->setUUIDValue(1, &mGroup);

	return ret;
}

bool LLPermissions::importXML(LLXMLNode* node)
{
	bool success = false;
	if (node)
	{
		success = true;
		LLXMLNodePtr sub_node;
		if (node->getChild("base_mask", sub_node))
			success = success && (4 == sub_node->getByteValue(4, (U8*)&mMaskBase));
		if (node->getChild("owner_mask", sub_node))
			success = success && (4 == sub_node->getByteValue(4, (U8*)&mMaskOwner));
		if (node->getChild("group_mask", sub_node))
			success = success && (4 == sub_node->getByteValue(4, (U8*)&mMaskGroup));
		if (node->getChild("everyone_mask", sub_node))
			success = success && (4 == sub_node->getByteValue(4, (U8*)&mMaskEveryone));
		if (node->getChild("next_owner_mask", sub_node))
			success = success && (4 == sub_node->getByteValue(4, (U8*)&mMaskNextOwner));

		if (node->getChild("creator_id", sub_node))
			success = success && (1 == sub_node->getUUIDValue(1, &mCreator));
		if (node->getChild("owner_id", sub_node))
			success = success && (1 == sub_node->getUUIDValue(1, &mOwner));
		if (node->getChild("last_owner_id", sub_node))
			success = success && (1 == sub_node->getUUIDValue(1, &mLastOwner));
		if (node->getChild("group_id", sub_node))
			success = success && (1 == sub_node->getUUIDValue(1, &mGroup));
		if (node->getChild("group_owned", sub_node))
			success = success && (1 == sub_node->getBoolValue(1, (BOOL*)&mIsGroupOwned));
		if (!success)
		{
			lldebugs << "LLPermissions::importXML() failed for node named '" 
				<< node->getName() << "'" << llendl;
		}
	}
	return success;
}

bool LLPermissions::operator==(const LLPermissions &rhs) const
{   
	return 
		(mCreator == rhs.mCreator) &&
		(mOwner == rhs.mOwner) &&
		(mLastOwner == rhs.mLastOwner ) &&
		(mGroup == rhs.mGroup ) &&
		(mMaskBase == rhs.mMaskBase ) &&
		(mMaskOwner == rhs.mMaskOwner ) &&
		(mMaskGroup == rhs.mMaskGroup ) &&
		(mMaskEveryone == rhs.mMaskEveryone ) &&
		(mMaskNextOwner == rhs.mMaskNextOwner ) &&
		(mIsGroupOwned == rhs.mIsGroupOwned);
}


bool LLPermissions::operator!=(const LLPermissions &rhs) const
{   
	return 
		(mCreator != rhs.mCreator) ||
		(mOwner != rhs.mOwner) ||
		(mLastOwner != rhs.mLastOwner ) ||
		(mGroup != rhs.mGroup ) ||
		(mMaskBase != rhs.mMaskBase ) ||
		(mMaskOwner != rhs.mMaskOwner ) ||
		(mMaskGroup != rhs.mMaskGroup ) ||
		(mMaskEveryone != rhs.mMaskEveryone ) ||
		(mMaskNextOwner != rhs.mMaskNextOwner) ||
		(mIsGroupOwned != rhs.mIsGroupOwned);
}

std::ostream& operator<<(std::ostream &s, const LLPermissions &perm)
{
	s << "{Creator=" 		<< perm.getCreator();
	s << ", Owner=" 		<< perm.getOwner();
	s << ", Group=" 		<< perm.getGroup();
	s << std::hex << ", BaseMask=0x" << perm.getMaskBase();
	s << ", OwnerMask=0x" << perm.getMaskOwner();
	s << ", EveryoneMask=0x" << perm.getMaskEveryone();
	s << ", GroupMask=0x" << perm.getMaskGroup();
	s << ", NextOwnerMask=0x" << perm.getMaskNextOwner() << std::dec;
	s << "}";
	return s;
}

template <>
void LLMetaClassT<LLPermissions>::reflectProperties(LLMetaClass& meta_class)
{
	reflectProperty(meta_class, "mCreator", &LLPermissions::mCreator);
	reflectProperty(meta_class, "mOwner", &LLPermissions::mOwner);
}

// virtual
const LLMetaClass& LLPermissions::getMetaClass() const
{
	return LLMetaClassT<LLPermissions>::instance();
}

///----------------------------------------------------------------------------
/// Class LLAggregatePermissions
///----------------------------------------------------------------------------

const LLAggregatePermissions LLAggregatePermissions::empty;


LLAggregatePermissions::LLAggregatePermissions()
{
	for(S32 i = 0; i < PI_COUNT; ++i)
	{
		mBits[i] = AP_EMPTY;
	}
}

LLAggregatePermissions::EValue LLAggregatePermissions::getValue(PermissionBit bit) const
{
	EPermIndex idx = perm2PermIndex(bit);
	EValue rv = AP_EMPTY;
	if(idx != PI_END)
	{
		rv = (LLAggregatePermissions::EValue)(mBits[idx]);
	}
	return rv;
}

// returns the bits compressed into a single byte: 00TTMMCC
// where TT = transfer, MM = modify, and CC = copy
// LSB is to the right
U8 LLAggregatePermissions::getU8() const
{
	U8 byte = mBits[PI_TRANSFER];
	byte <<= 2;
	byte |= mBits[PI_MODIFY];
	byte <<= 2;
	byte |= mBits[PI_COPY];
	return byte;
}

BOOL LLAggregatePermissions::isEmpty() const
{
	for(S32 i = 0; i < PI_END; ++i)
	{
		if(mBits[i] != AP_EMPTY)
		{
			return FALSE;
		}
	}
	return TRUE;
}

void LLAggregatePermissions::aggregate(PermissionMask mask)
{
	BOOL is_allowed = mask & PERM_COPY;
	aggregateBit(PI_COPY, is_allowed);
	is_allowed = mask & PERM_MODIFY;
	aggregateBit(PI_MODIFY, is_allowed);
	is_allowed = mask & PERM_TRANSFER;
	aggregateBit(PI_TRANSFER, is_allowed);
}

void LLAggregatePermissions::aggregate(const LLAggregatePermissions& ag)
{
	for(S32 idx = PI_COPY; idx != PI_END; ++idx)
	{
		aggregateIndex((EPermIndex)idx, ag.mBits[idx]);
	}
}

void LLAggregatePermissions::aggregateBit(EPermIndex idx, BOOL allowed)
{
	//if(AP_SOME == mBits[idx]) return; // P4 branch prediction optimization
	switch(mBits[idx])
	{
	case AP_EMPTY:
		mBits[idx] = allowed ? AP_ALL : AP_NONE;
		break;
	case AP_NONE:
		mBits[idx] = allowed ? AP_SOME: AP_NONE;
		break;
	case AP_SOME:
		// no-op
		break;
	case AP_ALL:
		mBits[idx] = allowed ? AP_ALL : AP_SOME;
		break;
	default:
		llwarns << "Bad aggregateBit " << (S32)idx << " "
				<< (allowed ? "true" : "false") << llendl;
		break;
	}
}

void LLAggregatePermissions::aggregateIndex(EPermIndex idx, U8 bits)
{
	switch(mBits[idx])
	{
	case AP_EMPTY:
		mBits[idx] = bits;
		break;
	case AP_NONE:
		switch(bits)
		{
		case AP_SOME:
		case AP_ALL:
			mBits[idx] = AP_SOME;
			break;
		case AP_EMPTY:
		case AP_NONE:
		default:
			// no-op
			break;
		}
		break;
	case AP_SOME:
		// no-op
		break;
	case AP_ALL:
		switch(bits)
		{
		case AP_NONE:
		case AP_SOME:
			mBits[idx] = AP_SOME;
			break;
		case AP_EMPTY:
		case AP_ALL:
		default:
			// no-op
			break;
		}
		break;
	default:
		llwarns << "Bad aggregate index " << (S32)idx << " "
				<<  (S32)bits << llendl;
		break;
	}
}

// static
LLAggregatePermissions::EPermIndex LLAggregatePermissions::perm2PermIndex(PermissionBit bit)
{
	EPermIndex idx = PI_END; // past any good value.
	switch(bit)
	{
	case PERM_COPY:
		idx = PI_COPY;
		break;
	case PERM_MODIFY:
		idx = PI_MODIFY;
		break;
	case PERM_TRANSFER:
		idx = PI_TRANSFER;
		break;
	default:
		break;
	}
	return idx;
}


void LLAggregatePermissions::packMessage(LLMessageSystem* msg, const char* field) const
{
	msg->addU8Fast(field, getU8());
}

void LLAggregatePermissions::unpackMessage(LLMessageSystem* msg, const char* block, const char* field, S32 block_num)
{
	const U8 TWO_BITS = 0x3; // binary 00000011
	U8 bits = 0;
	msg->getU8Fast(block, field, bits, block_num);
	mBits[PI_COPY] = bits & TWO_BITS;
	bits >>= 2;
	mBits[PI_MODIFY] = bits & TWO_BITS;
	bits >>= 2;
	mBits[PI_TRANSFER] = bits & TWO_BITS;
}

const LLString AGGREGATE_VALUES[4] =
	{
		LLString( "Empty" ),
		LLString( "None" ),
		LLString( "Some" ),
		LLString( "All" )
	};

std::ostream& operator<<(std::ostream &s, const LLAggregatePermissions &perm)
{
	s << "{PI_COPY=" << AGGREGATE_VALUES[perm.mBits[LLAggregatePermissions::PI_COPY]];
	s << ", PI_MODIFY=" << AGGREGATE_VALUES[perm.mBits[LLAggregatePermissions::PI_MODIFY]];
	s << ", PI_TRANSFER=" << AGGREGATE_VALUES[perm.mBits[LLAggregatePermissions::PI_TRANSFER]];
	s << "}";
	return s;
}

// This converts a permissions mask into a string for debugging use.
void mask_to_string(U32 mask, char* str)
{
	if (mask & PERM_MOVE)
	{
		*str = 'V';
	}
	else
	{
		*str = ' ';
	}	
	str++;

	if (mask & PERM_MODIFY)
	{
		*str = 'M';
	}
	else
	{
		*str = ' ';
	}	
	str++;
	
	if (mask & PERM_COPY)
	{
		*str = 'C';
	}
	else
	{
		*str = ' ';
	}	
	str++;
	
	if (mask & PERM_TRANSFER)
	{
		*str = 'T';
	}
	else
	{
		*str = ' ';
	}	
	str++;	
	*str = '\0';
}


///----------------------------------------------------------------------------
/// exported functions
///----------------------------------------------------------------------------
static const std::string PERM_CREATOR_ID_LABEL("creator_id");
static const std::string PERM_OWNER_ID_LABEL("owner_id");
static const std::string PERM_LAST_OWNER_ID_LABEL("last_owner_id");
static const std::string PERM_GROUP_ID_LABEL("group_id");
static const std::string PERM_IS_OWNER_GROUP_LABEL("is_owner_group");
static const std::string PERM_BASE_MASK_LABEL("base_mask");
static const std::string PERM_OWNER_MASK_LABEL("owner_mask");
static const std::string PERM_GROUP_MASK_LABEL("group_mask");
static const std::string PERM_EVERYONE_MASK_LABEL("everyone_mask");
static const std::string PERM_NEXT_OWNER_MASK_LABEL("next_owner_mask");

LLSD ll_create_sd_from_permissions(const LLPermissions& perm)
{
	LLSD rv;
	rv[PERM_CREATOR_ID_LABEL] = perm.getCreator();
	rv[PERM_OWNER_ID_LABEL] = perm.getOwner();
	rv[PERM_LAST_OWNER_ID_LABEL] = perm.getLastOwner();
	rv[PERM_GROUP_ID_LABEL] = perm.getGroup();
	rv[PERM_IS_OWNER_GROUP_LABEL] = perm.isGroupOwned();
	rv[PERM_BASE_MASK_LABEL] = (S32)perm.getMaskBase();
	rv[PERM_OWNER_MASK_LABEL] = (S32)perm.getMaskOwner();
	rv[PERM_GROUP_MASK_LABEL] = (S32)perm.getMaskGroup();
	rv[PERM_EVERYONE_MASK_LABEL] = (S32)perm.getMaskEveryone();
	rv[PERM_NEXT_OWNER_MASK_LABEL] = (S32)perm.getMaskNextOwner();
	return rv;
}

LLPermissions ll_permissions_from_sd(const LLSD& sd_perm)
{
	LLPermissions rv;
	rv.init(
		sd_perm[PERM_CREATOR_ID_LABEL].asUUID(),
		sd_perm[PERM_OWNER_ID_LABEL].asUUID(),
		sd_perm[PERM_LAST_OWNER_ID_LABEL].asUUID(),
		sd_perm[PERM_GROUP_ID_LABEL].asUUID());

	// We do a cast to U32 here since LLSD does not attempt to
	// represent unsigned ints.
	PermissionMask mask;
	mask = (U32)(sd_perm[PERM_BASE_MASK_LABEL].asInteger());
	rv.setMaskBase(mask);
	mask = (U32)(sd_perm[PERM_OWNER_MASK_LABEL].asInteger());
	rv.setMaskOwner(mask);
	mask = (U32)(sd_perm[PERM_EVERYONE_MASK_LABEL].asInteger());
	rv.setMaskEveryone(mask);
	mask = (U32)(sd_perm[PERM_GROUP_MASK_LABEL].asInteger());
	rv.setMaskGroup(mask);
	mask = (U32)(sd_perm[PERM_NEXT_OWNER_MASK_LABEL].asInteger());
	rv.setMaskNext(mask);
	rv.fix();
	return rv;
}