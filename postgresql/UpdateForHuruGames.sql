--
-- This script will update an existing MOSS database to be able to accept
-- H'uru client marker games, after support has been added to the server.
-- This simply adds the blob column to the the database and updates the
-- fetchnode() function.
--
--
--
-- Add column to the table if it does not exist

ALTER TABLE markergame
ADD COLUMN IF NOT EXISTS blob bytea;

COMMENT ON COLUMN public.markergame.blob
    IS 'Blob_1 (0x40000000)';

-- Now update the fetchnode() function.  No worries if it already esists.
-- FUNCTION: public.fetchnode(numeric)

-- DROP FUNCTION public.fetchnode(numeric);

CREATE OR REPLACE FUNCTION public.fetchnode(
    v_nodeid numeric,
    OUT v_nodetype integer,
    OUT v_createtime integer,
    OUT v_modifytime integer,
    OUT v_createagename text,
    OUT v_createageuuid character,
    OUT v_creatoracctid character,
    OUT v_creatorid numeric,
    OUT v_uuid_1 character,
    OUT v_uuid_2 character,
    OUT v_filename text,
    OUT v_int32_1 numeric,
    OUT v_int32_2 numeric,
    OUT v_int32_3 numeric,
    OUT v_uint32_1 numeric,
    OUT v_uint32_2 numeric,
    OUT v_uint32_3 numeric,
    OUT v_string64_1 text,
    OUT v_string64_2 text,
    OUT v_string64_3 text,
    OUT v_string64_4 text,
    OUT v_text_1 text,
    OUT v_type integer,
    OUT v_linkpoints text,
    OUT v_exists integer,
    OUT v_name text,
    OUT v_value text,
    OUT v_gender text,
    OUT v_online integer,
    OUT v_ki numeric,
    OUT v_blob bytea,
    OUT v_title text,
    OUT v_image bytea)
    RETURNS record
    LANGUAGE 'plpgsql'

    COST 100
    VOLATILE 
AS $BODY$
/* This function returns a node. Whee! */

DECLARE
  ntype integer;
  /* Junk vars  used so select * will work */
  id numeric;

  /* these are for time conversion */
  v_createtimecvt timestamp without time zone;
  v_modifytimecvt timestamp without time zone;
  
BEGIN
  /* Check to see if this node exists in master node table, and if it does get the type.
     If it does not exist, exit function.  In this case, nodetype will be <null>, so the
     caller can check this value before proceeding. */
  
  select type from nodes where nodeid = v_nodeid into ntype;
  if ntype is null then
    return; /* Node not found.  Exit function with null record returned. */
  end if;
  
  if ntype = 3 then /* age node */
    select 3, * from age where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_creatoracctid, v_creatorid, v_uuid_1, v_uuid_2, v_filename;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 33 then /* ageinfo node */
    select 33, * from ageinfo where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_creatoracctid, v_creatorid, v_int32_1, v_int32_2, v_int32_3, v_uint32_1,
      v_uint32_2, v_uint32_3, v_uuid_1, v_uuid_2, v_string64_2, v_string64_3, v_string64_4, v_text_1;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 34 then /* ageinfolist node */
    select 34, * from ageinfolist where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_creatoracctid, v_creatorid, v_type;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 28 then /* agelink node */
    select 28, * from agelink where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_createageuuid, v_createagename, v_creatoracctid, v_creatorid, v_int32_1, v_int32_2, v_linkpoints;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;
  
  elseif ntype = 29 then /* chronicle node */
    select 29, * from chronicle where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_createageuuid, v_createagename, v_creatoracctid, v_creatorid, v_type, v_name, v_value;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 22 then /* folder node */
    select 22, * from folder where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_createageuuid, v_createagename, v_creatoracctid, v_creatorid, v_type, v_name;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 25 then /* image node */
    select 25, * from image where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_createageuuid, v_createagename, v_creatoracctid, v_creatorid, v_exists, v_name, v_image;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 35 then /* markergame node ( UU/PotS calls this MarkerList) */
    select 35, * from markergame where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_createageuuid, v_createagename, v_creatoracctid, v_creatorid, v_name, v_uuid_1, v_blob;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 2 then /* player node */
    select 2, * from player where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_creatoracctid, v_creatorid, v_int32_1, v_int32_2, v_uint32_1, v_uuid_1, v_uuid_2, v_gender, v_name;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 23 then /* playerinfo node */
    select 23, * from playerinfo where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_creatoracctid, v_creatorid, v_online, v_ki, v_uuid_1, v_string64_1, v_name;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 30 then /* playerinfolist */
    select 30, * from playerinfolist where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_creatoracctid, v_creatorid, v_type;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 27 then /* sdl node */
    select 27, * from sdl where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_createageuuid, v_createagename, v_creatoracctid, v_creatorid, v_int32_1, v_name, v_blob;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 24 then /* system node */
    select 24, * from system where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_creatoracctid, v_creatorid;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;

  elseif ntype = 26 then /* textnote node */
    select 26, * from textnote where nodeid = v_nodeid into v_nodetype, id, v_createtimecvt, v_modifytimecvt, v_createageuuid, v_createagename, v_creatoracctid, v_creatorid, v_int32_1, v_int32_2, v_title, v_value;
    v_createtime := trunc(EXTRACT(EPOCH FROM v_createtimecvt));
    v_modifytime := trunc(EXTRACT(EPOCH FROM v_modifytimecvt));
    return;
  end if;

  return; /* fall though return - we should not get here */

END;
$BODY$;

ALTER FUNCTION public.fetchnode(numeric)
    OWNER TO moss;

-- done

