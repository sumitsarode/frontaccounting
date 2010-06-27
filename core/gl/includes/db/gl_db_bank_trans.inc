<?php
/**********************************************************************
    Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
//----------------------------------------------------------------------------------

// add a bank transaction
// $amount is in $currency
// $date_ is display date (non-sql)

function add_bank_trans($type, $trans_no, $bank_act, $ref, $date_,
	$amount, $person_type_id, $person_id, $currency="", $err_msg="", $rate=0)
{
	$sqlDate = date2sql($date_);

	// convert $amount to the bank's currency
	if ($currency != "")
	{
    	$bank_account_currency = get_bank_account_currency($bank_act);
    	if ($rate == 0)
    		$to_bank_currency = get_exchange_rate_from_to($currency, $bank_account_currency, $date_);
    	else
    		$to_bank_currency = 1 / $rate;
    	$amount_bank = ($amount / $to_bank_currency);
	}
	else
		$amount_bank = $amount;


	// Also store the rate to the home
	//$BankToHomeCurrencyRate = get_exchange_rate_to_home_currency($bank_account_currency, $date_);

	$sql = "INSERT INTO ".TB_PREF."bank_trans (type, trans_no, bank_act, ref,
		trans_date, amount, person_type_id, person_id) ";

	$sql .= "VALUES ($type, $trans_no, '$bank_act', ".db_escape($ref).", '$sqlDate',
		".db_escape($amount_bank).", ".db_escape($person_type_id)
		.", ". db_escape($person_id).")";

	if ($err_msg == "")
		$err_msg = "The bank transaction could not be inserted";

	db_query($sql, $err_msg);
}

//----------------------------------------------------------------------------------------

function exists_bank_trans($type, $type_no)
{
	$sql = "SELECT trans_no FROM ".TB_PREF."bank_trans WHERE type=".db_escape($type)
		." AND trans_no=".db_escape($type_no);
	$result = db_query($sql, "Cannot retreive a bank transaction");

    return (db_num_rows($result) > 0);
}

//----------------------------------------------------------------------------------------

function get_bank_trans($type, $trans_no=null, $person_type_id=null, $person_id=null)
{
	$sql = "SELECT *, bank_account_name, account_code, bank_curr_code
		FROM ".TB_PREF."bank_trans, ".TB_PREF."bank_accounts
		WHERE ".TB_PREF."bank_accounts.id=".TB_PREF."bank_trans.bank_act ";
	if ($type != null)
		$sql .= " AND type=".db_escape($type);
	if ($trans_no != null)
		$sql .= " AND ".TB_PREF."bank_trans.trans_no = ".db_escape($trans_no);
	if ($person_type_id != null)
		$sql .= " AND ".TB_PREF."bank_trans.person_type_id = ".db_escape($person_type_id);
	if ($person_id != null)
		$sql .= " AND ".TB_PREF."bank_trans.person_id = ".db_escape($person_id);
	$sql .= " ORDER BY trans_date, ".TB_PREF."bank_trans.id";

	return db_query($sql, "query for bank transaction");
}

//----------------------------------------------------------------------------------------

function get_bank_trans_for_bank_account($bank_account, $from, $to)
{
	$from = date2sql($from);
	$to = date2sql($to);
	$sql = "SELECT ".TB_PREF."bank_trans.* FROM ".TB_PREF."bank_trans
		WHERE ".TB_PREF."bank_trans.bank_act = ".db_escape($bank_account) . "
		AND trans_date >= '$from'
		AND trans_date <= '$to'
		ORDER BY trans_date,".TB_PREF."bank_trans.id";

	return db_query($sql,"The transactions for '" . $bank_account . "' could not be retrieved");
}

//----------------------------------------------------------------------------------------

function get_balance_before_for_bank_account($bank_account, $from)
{
	$from = date2sql($from);
	$sql = "SELECT SUM(amount) FROM ".TB_PREF."bank_trans WHERE bank_act="
		.db_escape($bank_account) . "
		AND trans_date < '$from'";
	$before_qty = db_query($sql, "The starting balance on hand could not be calculated");
	$bfw_row = db_fetch_row($before_qty);
	return $bfw_row[0];
}
//----------------------------------------------------------------------------------------

function get_gl_trans_value($account, $type, $trans_no)
{
	$sql = "SELECT SUM(amount) FROM ".TB_PREF."gl_trans WHERE account="
	.db_escape($account)." AND type=".db_escape($type)
	." AND type_no=".db_escape($trans_no);

	$result = db_query($sql, "query for gl trans value");

	$row = db_fetch_row($result);
	return $row[0];
}

//----------------------------------------------------------------------------------------

function void_bank_trans($type, $type_no, $nested=false)
{
	if (!$nested)
		begin_transaction();

	$sql = "UPDATE ".TB_PREF."bank_trans SET amount=0
		WHERE type=".db_escape($type)." AND trans_no=".db_escape($type_no);

	$result = db_query($sql, "could not void bank transactions for type=$type and trans_no=$type_no");

	void_gl_trans($type, $type_no, true);

	// in case it's a customer trans - probably better to check first
	void_cust_allocations($type, $type_no);
	void_customer_trans($type, $type_no);

	// in case it's a supplier trans - probably better to check first
	void_supp_allocations($type, $type_no);
	void_supp_trans($type, $type_no);

	void_trans_tax_details($type, $type_no);

	if (!$nested)
		commit_transaction();
}

//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------

function clear_bank_trans($type, $type_no, $nested=false)
{
	global $Refs;
	
	if (!$nested)
		begin_transaction();

	$sql = "DELETE FROM ".TB_PREF."bank_trans 
		WHERE type=".db_escape($type)." AND trans_no=".db_escape($type_no);

	$result = db_query($sql, "could not clear bank transactions for type=$type and trans_no=$type_no");

	clear_gl_trans($type, $type_no, true);

	// in case it's a customer trans - probably better to check first
	void_cust_allocations($type, $type_no);
	clear_customer_trans($type, $type_no);

	// in case it's a supplier trans - probably better to check first
	void_supp_allocations($type, $type_no);
	clear_supp_trans($type, $type_no);

	clear_trans_tax_details($type, $type_no);

	//Delete the reference
	$Refs->delete($type, $type_no);	
	
	if (!$nested)
		commit_transaction();
}


?>