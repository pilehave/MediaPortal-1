#region Copyright (C) 2005-2011 Team MediaPortal

// Copyright (C) 2005-2011 Team MediaPortal
// http://www.team-mediaportal.com
// 
// MediaPortal is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
// 
// MediaPortal is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with MediaPortal. If not, see <http://www.gnu.org/licenses/>.

#endregion

using System;
using TvLibrary.Interfaces;
using TvDatabase;

namespace TvService
{
  /// <summary>
  /// Class which can be used to sort Cards bases on priority
  /// </summary>
  public class CardDetail : IComparable<CardDetail>
  {
    private readonly int _cardId;
    private readonly Card _card;
    private readonly IChannel _detail;
    private int _priority;
    private bool _sameTransponder;
    private int _numberOfOtherUsers;

    /// <summary>
    /// ctor
    /// </summary>
    /// <param name="id">card id</param>
    /// <param name="card">card dataaccess object</param>
    /// <param name="detail">tuning detail</param>
    /// <param name="sameTransponder">indicates whether it is the same transponder</param>
    public CardDetail(int id, Card card, IChannel detail, bool sameTransponder, int numberOfOtherUsers)
    {
      _sameTransponder = sameTransponder;
      _cardId = id;
      _card = card;
      _detail = detail;
      _priority = _card.Priority;
      _numberOfOtherUsers = numberOfOtherUsers;
    }

    /// <summary>
    /// gets the id of the card
    /// </summary>
    public int Id
    {
      get { return _cardId; }
    }

    /// <summary>
    /// gets/sets the priority
    /// </summary>
    /// <value>The priority.</value>
    public int Priority
    {
      get { return _priority; }
    }

    /// <summary>
    /// gets the card
    /// </summary>
    public Card Card
    {
      get { return _card; }
    }

    /// <summary>
    /// gets the tuning detail
    /// </summary>
    public IChannel TuningDetail
    {
      get { return _detail; }
    }

    /// <summary>
    /// returns if it is the same transponder
    /// </summary>
    public bool SameTransponder
    {
      get { return _sameTransponder; }
    }

    /// <summary>
    /// gets the number of other users
    /// </summary>
    public int NumberOfOtherUsers
    {
      get { return _numberOfOtherUsers; }
    }

    #region IComparable<CardInfo> Members

    // higher priority means that this one should be more to the front of the list
    public int CompareTo(CardDetail other)
    {
      if (SameTransponder == other.SameTransponder)
      {
        if (!SameTransponder && (NumberOfOtherUsers != other.NumberOfOtherUsers))
        {
          if (NumberOfOtherUsers > other.NumberOfOtherUsers)
            return 1;
          if (NumberOfOtherUsers < other.NumberOfOtherUsers)
            return -1;
          return 0;
        }

        if (Priority > other.Priority)
          return -1;
        if (Priority < other.Priority)
          return 1;
        return 0;
      }

      if (SameTransponder)
        return -1;
      return 1;
    }

    #endregion
  }
}